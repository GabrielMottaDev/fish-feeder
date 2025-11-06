#ifndef FEEDING_CONTROLLER_H
#define FEEDING_CONTROLLER_H

#include <Arduino.h>
#include "stepper_motor.h"
#include "config.h"

/**
 * FeedingController Class
 * 
 * Handles all feeding-related logic and operations.
 * This class separates feeding business logic from low-level motor control,
 * making the system more modular and easier to maintain.
 * 
 * Uses global configuration from config.h for feeding parameters.
 */
class FeedingController {
private:
    StepperMotor* motor;                // Reference to stepper motor
    bool isInitialized;                 // Initialization status
    
public:
    // Constructor and initialization
    FeedingController(StepperMotor* stepperMotor);
    bool begin();
    
    // Main feeding operations
    bool dispenseFood(int portions);
    bool dispenseFoodAsync(int portions);
    
    // Calibration and testing
    void calibrateFeeder();
    void testFeeder(int testPortions = 1);
    
    // Status and information
    void printFeedingStatus() const;
    bool isReady() const;
    
    // Configuration helpers
    static int getMaxPortions();
    static int getMinPortions();
    static float getPortionRotation();
    static void printFeedingConfiguration();
};

#endif // FEEDING_CONTROLLER_H