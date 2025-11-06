#include "feeding_controller.h"

/**
 * Constructor: Initialize feeding controller with stepper motor reference
 * 
 * @param stepperMotor: Pointer to initialized StepperMotor instance
 */
FeedingController::FeedingController(StepperMotor* stepperMotor) 
    : motor(stepperMotor), isInitialized(false) {
}

/**
 * Initialize the feeding controller
 * 
 * @return: true if initialization successful, false otherwise
 */
bool FeedingController::begin() {
    if (!motor) {
        Serial.println(F("ERROR: No stepper motor provided to FeedingController"));
        return false;
    }
    
    if (!motor->isReady()) {
        Serial.println(F("ERROR: Stepper motor not ready for feeding operations"));
        return false;
    }
    
    isInitialized = true;
    Serial.println(F("FeedingController initialized successfully"));
    
    // Print configuration for reference
    printFeedingConfiguration();
    
    return true;
}

/**
 * Dispense food portions (blocking operation)
 * 
 * @param portions: Number of portions to dispense
 * @return: true if successful, false if error
 */
bool FeedingController::dispenseFood(int portions) {
    if (!isInitialized || !motor) {
        Serial.println(F("ERROR: FeedingController not initialized"));
        return false;
    }
    
    // Validate portion count
    if (!isValidPortionCount(portions)) {
        Serial.print(F("ERROR: Invalid portion count ("));
        Serial.print(portions);
        Serial.print(F("). Must be between "));
        Serial.print(MIN_FOOD_PORTIONS);
        Serial.print(F(" and "));
        Serial.println(MAX_FOOD_PORTIONS);
        return false;
    }
    
    Serial.print(F("Dispensing "));
    Serial.print(portions);
    Serial.print(F(" food portion(s)..."));
    
    // Calculate steps needed
    int steps = portionsToSteps(portions);
    
    Serial.print(F(" ("));
    Serial.print(steps);
    Serial.println(F(" steps)"));
    
    // Execute movement
    motor->stepClockwise(steps);
    
    Serial.println(F("Food dispensing completed successfully"));
    return true;
}

/**
 * Dispense food portions (non-blocking operation)
 * 
 * @param portions: Number of portions to dispense
 * @return: true if movement started successfully, false if error
 */
bool FeedingController::dispenseFoodAsync(int portions) {
    if (!isInitialized || !motor) {
        Serial.println(F("ERROR: FeedingController not initialized"));
        return false;
    }
    
    // Validate portion count
    if (!isValidPortionCount(portions)) {
        Serial.print(F("ERROR: Invalid portion count ("));
        Serial.print(portions);
        Serial.print(F("). Must be between "));
        Serial.print(MIN_FOOD_PORTIONS);
        Serial.print(F(" and "));
        Serial.println(MAX_FOOD_PORTIONS);
        return false;
    }
    
    Serial.print(F("Starting async dispensing of "));
    Serial.print(portions);
    Serial.println(F(" food portion(s)..."));
    
    // Calculate steps and start async movement
    int steps = portionsToSteps(portions);
    long currentPos = motor->getCurrentPosition();
    motor->moveToPositionAsync(currentPos + steps);
    
    return true;
}

/**
 * Calibrate feeder by performing full revolution
 * Used for initial setup and mechanical testing
 */
void FeedingController::calibrateFeeder() {
    if (!isInitialized || !motor) {
        Serial.println(F("ERROR: Cannot calibrate - FeedingController not initialized"));
        return;
    }
    
    Serial.println(F("Starting feeder calibration..."));
    Serial.println(F("Motor will complete 1 full revolution for mechanical testing"));
    
    // Reset position for calibration
    motor->resetPosition();
    
    // Rotate one full revolution clockwise
    motor->rotateClockwise(1.0);
    
    Serial.println(F("Calibration completed successfully"));
    Serial.print(F("Final position: "));
    Serial.print(motor->getCurrentPosition());
    Serial.println(F(" steps"));
    
    // Show feeding equivalents
    float portions = (float)motor->getCurrentPosition() / portionsToSteps(1);
    Serial.print(F("Equivalent to approximately "));
    Serial.print(portions, 1);
    Serial.println(F(" food portions"));
}

/**
 * Test feeder with specified number of portions
 * 
 * @param testPortions: Number of portions for testing (default 1)
 */
void FeedingController::testFeeder(int testPortions) {
    Serial.print(F("Testing feeder with "));
    Serial.print(testPortions);
    Serial.println(F(" portion(s)"));
    
    if (dispenseFood(testPortions)) {
        Serial.println(F("Feeder test completed successfully"));
    } else {
        Serial.println(F("Feeder test failed"));
    }
}

/**
 * Print current feeding status and motor information
 */
void FeedingController::printFeedingStatus() const {
    Serial.println(F("=== Feeding Controller Status ==="));
    Serial.print(F("Initialized: "));
    Serial.println(isInitialized ? F("Yes") : F("No"));
    Serial.print(F("Motor Ready: "));
    Serial.println((motor && motor->isReady()) ? F("Yes") : F("No"));
    
    if (motor && motor->isReady()) {
        Serial.print(F("Current Position: "));
        Serial.print(motor->getCurrentPosition());
        Serial.println(F(" steps"));
        
        // Calculate equivalent portions at current position
        int currentSteps = motor->getCurrentPosition();
        float equivalentPortions = (float)currentSteps / portionsToSteps(1);
        Serial.print(F("Position equivalent: "));
        Serial.print(equivalentPortions, 2);
        Serial.println(F(" portions"));
        
        Serial.print(F("Motor Running: "));
        Serial.println(motor->isRunning() ? F("Yes") : F("No"));
    }
    
    Serial.println(F("================================"));
}

/**
 * Check if feeding controller is ready for operations
 * 
 * @return: true if ready, false otherwise
 */
bool FeedingController::isReady() const {
    return isInitialized && motor && motor->isReady();
}

/**
 * Get maximum allowed portions (static helper)
 * 
 * @return: Maximum portions from configuration
 */
int FeedingController::getMaxPortions() {
    return MAX_FOOD_PORTIONS;
}

/**
 * Get minimum allowed portions (static helper)
 * 
 * @return: Minimum portions from configuration
 */
int FeedingController::getMinPortions() {
    return MIN_FOOD_PORTIONS;
}

/**
 * Get portion rotation value (static helper)
 * 
 * @return: Rotation per portion from configuration
 */
float FeedingController::getPortionRotation() {
    return FOOD_PORTION_ROTATION;
}

/**
 * Print feeding configuration (static helper)
 */
void FeedingController::printFeedingConfiguration() {
    Serial.println(F("=== Feeding Configuration ==="));
    Serial.print(F("Portion Rotation: "));
    Serial.print(FOOD_PORTION_ROTATION, 3);
    Serial.println(F(" revolutions"));
    Serial.print(F("Steps per Portion: "));
    Serial.print(portionsToSteps(1));
    Serial.println(F(" steps"));
    Serial.print(F("Min/Max Portions: "));
    Serial.print(MIN_FOOD_PORTIONS);
    Serial.print(F(" - "));
    Serial.println(MAX_FOOD_PORTIONS);
    Serial.print(F("Steps per Revolution: "));
    Serial.println(STEPS_PER_REVOLUTION);
    Serial.println(F("============================="));
}