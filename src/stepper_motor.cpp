#include "stepper_motor.h"
#include "config.h"
#include <Preferences.h>

// Preferences object for NVRAM storage
Preferences motorPreferences;

/**
 * Constructor: Initialize stepper motor with pin configuration
 * 
 * @param in1, in2, in3, in4: ULN2003 control pins (IN1-IN4)
 * @param stepsPerRev: Steps per revolution (default 2048 for 28BYJ-48 in half-step mode)
 */
StepperMotor::StepperMotor(int in1, int in2, int in3, int in4, int stepsPerRev) 
    : pin1(in1), pin2(in2), pin3(in3), pin4(in4), 
      stepsPerRevolution(stepsPerRev), stepper(nullptr), isInitialized(false),
      maxSpeed(1200.0), acceleration(800.0), motorDirectionClockwise(DEFAULT_MOTOR_CLOCKWISE) {
}

/**
 * Destructor: Clean up stepper instance and disable motor
 */
StepperMotor::~StepperMotor() {
    if (stepper) {
        disableMotor();
        delete stepper;
    }
}

/**
 * Initialize the stepper motor
 * 
 * @return true if initialization successful, false otherwise
 */
bool StepperMotor::begin() {
    Serial.println(F("Initializing Stepper Motor (28BYJ-48) with AccelStepper..."));
    
    // Load motor direction from NVRAM
    motorPreferences.begin("motor", false);
    motorDirectionClockwise = motorPreferences.getBool(MOTOR_DIRECTION_NVRAM_KEY, DEFAULT_MOTOR_CLOCKWISE);
    motorPreferences.end();
    
    Serial.print(F("Motor direction loaded from NVRAM: "));
    Serial.println(motorDirectionClockwise ? F("CLOCKWISE (CW)") : F("COUNTER-CLOCKWISE (CCW)"));
    
    // Create AccelStepper instance with FULL4WIRE interface
    // Pin order for ULN2003: IN1, IN3, IN2, IN4 (proper sequence)
    stepper = new AccelStepper(AccelStepper::FULL4WIRE, pin1, pin3, pin2, pin4);
    
    if (!stepper) {
        Serial.println(F("ERROR: Failed to create AccelStepper instance"));
        return false;
    }
    
    // Initialize pins
    initializePins();
    
    // Set default parameters
    stepper->setMaxSpeed(maxSpeed);
    stepper->setAcceleration(acceleration);
    stepper->setCurrentPosition(0);
    
    isInitialized = true;
    Serial.println(F("AccelStepper Motor initialized successfully"));
    Serial.print(F("Pin Configuration - IN1: "));
    Serial.print(pin1);
    Serial.print(F(", IN2: "));
    Serial.print(pin2);
    Serial.print(F(", IN3: "));
    Serial.print(pin3);
    Serial.print(F(", IN4: "));
    Serial.println(pin4);
    Serial.print(F("Max Speed: "));
    Serial.print(maxSpeed);
    Serial.print(F(" steps/sec, Acceleration: "));
    Serial.print(acceleration);
    Serial.println(F(" steps/sec²"));
    
    return true;
}

/**
 * Initialize motor control pins as outputs
 */
void StepperMotor::initializePins() {
    pinMode(pin1, OUTPUT);
    pinMode(pin2, OUTPUT);
    pinMode(pin3, OUTPUT);
    pinMode(pin4, OUTPUT);
    
    // Start with all pins low
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
    digitalWrite(pin3, LOW);
    digitalWrite(pin4, LOW);
}

/**
 * Disable motor by setting all pins to LOW
 * This reduces power consumption and heat
 */
void StepperMotor::disableMotor() {
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
    digitalWrite(pin3, LOW);
    digitalWrite(pin4, LOW);
}

/**
 * Set maximum speed in steps per second
 * 
 * @param speed: Maximum speed in steps/second
 */
void StepperMotor::setMaxSpeed(float speed) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    maxSpeed = speed;
    stepper->setMaxSpeed(speed);
    Serial.print(F("Max speed set to "));
    Serial.print(speed);
    Serial.println(F(" steps/second"));
}

/**
 * Set acceleration in steps per second squared
 * 
 * @param accel: Acceleration in steps/second²
 */
void StepperMotor::setAcceleration(float accel) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    acceleration = accel;
    stepper->setAcceleration(accel);
    Serial.print(F("Acceleration set to "));
    Serial.print(accel);
    Serial.println(F(" steps/second²"));
}

/**
 * Set constant speed for runSpeed() operation
 * 
 * @param speed: Constant speed in steps/second
 */
void StepperMotor::setSpeed(float speed) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    stepper->setSpeed(speed);
    Serial.print(F("Constant speed set to "));
    Serial.print(speed);
    Serial.println(F(" steps/second"));
}

/**
 * Set motor rotation direction for feeding operations
 * 
 * @param clockwise: true = clockwise, false = counter-clockwise
 */
void StepperMotor::setMotorDirection(bool clockwise) {
    motorDirectionClockwise = clockwise;
    
    // Save to NVRAM for persistence
    motorPreferences.begin("motor", false);
    motorPreferences.putBool(MOTOR_DIRECTION_NVRAM_KEY, clockwise);
    motorPreferences.end();
    
    Serial.print(F("Motor direction set to: "));
    Serial.println(clockwise ? F("CLOCKWISE (CW)") : F("COUNTER-CLOCKWISE (CCW)"));
    Serial.println(F("Direction saved to NVRAM"));
}

/**
 * Get current motor rotation direction
 * 
 * @return true if clockwise, false if counter-clockwise
 */
bool StepperMotor::getMotorDirection() const {
    return motorDirectionClockwise;
}

/**
 * Step motor clockwise by specified number of steps (blocking)
 * 
 * @param steps: Number of steps to move
 */
void StepperMotor::stepClockwise(int steps) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    Serial.print(F("Moving "));
    Serial.print(steps);
    Serial.println(F(" steps clockwise"));
    
    long currentPos = stepper->currentPosition();
    
    // Apply direction: if motor direction is CCW, invert the steps
    int adjustedSteps = motorDirectionClockwise ? steps : -steps;
    stepper->moveTo(currentPos + adjustedSteps);
    
    // Run until target is reached
    while (stepper->distanceToGo() != 0) {
        stepper->run();
    }
    
    // Disable motor after movement to save power
    disableMotor();
}

/**
 * Step motor counter-clockwise by specified number of steps (blocking)
 * 
 * @param steps: Number of steps to move
 */
void StepperMotor::stepCounterClockwise(int steps) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    Serial.print(F("Moving "));
    Serial.print(steps);
    Serial.println(F(" steps counter-clockwise"));
    
    long currentPos = stepper->currentPosition();
    
    // Apply direction: if motor direction is CCW, invert the steps
    int adjustedSteps = motorDirectionClockwise ? -steps : steps;
    stepper->moveTo(currentPos + adjustedSteps);
    
    // Run until target is reached
    while (stepper->distanceToGo() != 0) {
        stepper->run();
    }
    
    // Disable motor after movement to save power
    disableMotor();
}

/**
 * Rotate motor clockwise by specified number of revolutions
 * 
 * @param revolutions: Number of full revolutions
 */
void StepperMotor::rotateClockwise(float revolutions) {
    int steps = (int)(revolutions * stepsPerRevolution);
    stepClockwise(steps);
}

/**
 * Rotate motor counter-clockwise by specified number of revolutions
 * 
 * @param revolutions: Number of full revolutions
 */
void StepperMotor::rotateCounterClockwise(float revolutions) {
    int steps = (int)(revolutions * stepsPerRevolution);
    stepCounterClockwise(steps);
}

/**
 * Move motor to absolute position (blocking)
 * 
 * @param targetSteps: Target position in steps
 */
void StepperMotor::moveToPosition(long targetSteps) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    Serial.print(F("Moving to position: "));
    Serial.println(targetSteps);
    
    stepper->moveTo(targetSteps);
    
    // Run until target is reached
    while (stepper->distanceToGo() != 0) {
        stepper->run();
    }
    
    Serial.println(F("Target position reached"));
    disableMotor();
}

/**
 * Move motor to absolute position (non-blocking)
 * 
 * @param targetSteps: Target position in steps
 */
void StepperMotor::moveToPositionAsync(long targetSteps) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    Serial.print(F("Setting target position: "));
    Serial.println(targetSteps);
    
    stepper->moveTo(targetSteps);
}

/**
 * Run motor to target position (call in loop for non-blocking)
 * 
 * @return true if still running, false if target reached
 */
bool StepperMotor::runToPosition() {
    if (!isInitialized || !stepper) {
        return false;
    }
    
    bool stillRunning = stepper->run();
    if (!stillRunning) {
        disableMotor();
    }
    return stillRunning;
}

/**
 * Run motor at constant speed (call in loop)
 * 
 * @return true if step was taken, false otherwise
 */
bool StepperMotor::runSpeed() {
    if (!isInitialized || !stepper) {
        return false;
    }
    
    return stepper->runSpeed();
}

/**
 * Main run function - call frequently in loop for non-blocking operation
 */
void StepperMotor::run() {
    if (isInitialized && stepper) {
        stepper->run();
    }
}

/**
 * Get current position in steps
 * 
 * @return Current position
 */
long StepperMotor::getCurrentPosition() const {
    if (!isInitialized || !stepper) {
        return 0;
    }
    return stepper->currentPosition();
}

/**
 * Reset position counter to zero
 */
void StepperMotor::resetPosition() {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    stepper->setCurrentPosition(0);
    Serial.println(F("Position reset to zero"));
}

/**
 * Set current position to specified value
 * 
 * @param position: New position value
 */
void StepperMotor::setCurrentPosition(long position) {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    stepper->setCurrentPosition(position);
    Serial.print(F("Position set to "));
    Serial.println(position);
}

/**
 * Get target position
 * 
 * @return Target position in steps
 */
long StepperMotor::getTargetPosition() const {
    if (!isInitialized || !stepper) {
        return 0;
    }
    return stepper->targetPosition();
}

/**
 * Get distance to target position
 * 
 * @return Steps remaining to target
 */
long StepperMotor::distanceToGo() const {
    if (!isInitialized || !stepper) {
        return 0;
    }
    return stepper->distanceToGo();
}

/**
 * Check if motor is currently running
 * 
 * @return true if motor is moving
 */
bool StepperMotor::isRunning() const {
    if (!isInitialized || !stepper) {
        return false;
    }
    return stepper->isRunning();
}

/**
 * Stop motor and disable coils
 */
void StepperMotor::stop() {
    disableMotor();
    Serial.println(F("Motor stopped"));
}

/**
 * Check if motor is ready for operation
 * 
 * @return true if initialized and ready
 */
bool StepperMotor::isReady() const {
    return isInitialized && (stepper != nullptr);
}

/**
 * Print current motor status to Serial
 */
void StepperMotor::printStatus() const {
    Serial.println(F("=== AccelStepper Motor Status ==="));
    Serial.print(F("Initialized: "));
    Serial.println(isInitialized ? F("Yes") : F("No"));
    
    if (isInitialized && stepper) {
        Serial.print(F("Current Position: "));
        Serial.print(stepper->currentPosition());
        Serial.println(F(" steps"));
        Serial.print(F("Target Position: "));
        Serial.print(stepper->targetPosition());
        Serial.println(F(" steps"));
        Serial.print(F("Distance to Go: "));
        Serial.print(stepper->distanceToGo());
        Serial.println(F(" steps"));
        Serial.print(F("Is Running: "));
        Serial.println(stepper->isRunning() ? F("Yes") : F("No"));
        Serial.print(F("Max Speed: "));
        Serial.print(maxSpeed);
        Serial.println(F(" steps/sec"));
        Serial.print(F("Acceleration: "));
        Serial.print(acceleration);
        Serial.println(F(" steps/sec²"));
        Serial.print(F("Motor Direction: "));
        Serial.println(motorDirectionClockwise ? F("CLOCKWISE (CW)") : F("COUNTER-CLOCKWISE (CCW)"));
    }
    
    Serial.print(F("Steps per Revolution: "));
    Serial.println(stepsPerRevolution);
    Serial.print(F("Pin Configuration: IN1="));
    Serial.print(pin1);
    Serial.print(F(", IN2="));
    Serial.print(pin2);
    Serial.print(F(", IN3="));
    Serial.print(pin3);
    Serial.print(F(", IN4="));
    Serial.println(pin4);
    Serial.println(F("================================"));
}

/**
 * Enable high performance mode for maximum speed and torque
 * Optimizes settings for fast, powerful operation
 */
void StepperMotor::enableHighPerformanceMode() {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    Serial.println(F("Enabling HIGH PERFORMANCE mode..."));
    
    // Maximum reliable speed for 28BYJ-48 (can go up to 1500-2000 steps/sec)
    maxSpeed = 1500.0f;
    stepper->setMaxSpeed(maxSpeed);
    
    // High acceleration for rapid start/stop
    acceleration = 1000.0f;
    stepper->setAcceleration(acceleration);
    
    Serial.print(F("✓ Max Speed: "));
    Serial.print(maxSpeed);
    Serial.print(F(" steps/sec, Acceleration: "));
    Serial.print(acceleration);
    Serial.println(F(" steps/sec² (HIGH PERFORMANCE)"));
}

/**
 * Enable power saving mode for energy efficiency
 * Reduces speed/acceleration to minimize power consumption
 */
void StepperMotor::enablePowerSavingMode() {
    if (!isInitialized || !stepper) {
        Serial.println(F("ERROR: Motor not initialized"));
        return;
    }
    
    Serial.println(F("Enabling POWER SAVING mode..."));
    
    // Conservative speed for power efficiency
    maxSpeed = 500.0f;
    stepper->setMaxSpeed(maxSpeed);
    
    // Gentle acceleration to reduce power spikes
    acceleration = 250.0f;
    stepper->setAcceleration(acceleration);
    
    Serial.print(F("✓ Max Speed: "));
    Serial.print(maxSpeed);
    Serial.print(F(" steps/sec, Acceleration: "));
    Serial.print(acceleration);
    Serial.println(F(" steps/sec² (POWER SAVING)"));
}

/**
 * Perform a full revolution for testing/calibration purposes
 * Generic method that can be used for various calibration needs
 */
void StepperMotor::performFullRevolution() {
    if (!isInitialized) {
        Serial.println(F("ERROR: Cannot perform revolution - motor not initialized"));
        return;
    }
    
    Serial.println(F("Performing full revolution..."));
    
    // Store current position
    long startPosition = getCurrentPosition();
    
    // Rotate one full revolution clockwise
    rotateClockwise(1.0);
    
    long endPosition = getCurrentPosition();
    Serial.println(F("Full revolution completed"));
    Serial.print(F("Start position: "));
    Serial.print(startPosition);
    Serial.print(F(" steps, End position: "));
    Serial.print(endPosition);
    Serial.print(F(" steps, Total steps: "));
    Serial.println(endPosition - startPosition);
}

