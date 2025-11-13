#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <Arduino.h>
#include <AccelStepper.h>

/**
 * StepperMotor Class
 * 
 * Manages a 28BYJ-48 stepper motor with ULN2003 driver
 * Uses AccelStepper library for smooth acceleration/deceleration control
 * 
 * Hardware Configuration:
 * - Motor: 28BYJ-48 (2048 steps per revolution in half-step mode)
 * - Driver: ULN2003 
 * - Pins: IN1, IN2, IN3, IN4 configurable via constructor
 */
class StepperMotor {
private:
    AccelStepper* stepper;               // AccelStepper library instance
    int stepsPerRevolution;              // Steps per full revolution
    int pin1, pin2, pin3, pin4;         // Motor control pins
    bool isInitialized;                  // Initialization status
    float maxSpeed;                      // Maximum speed in steps/second
    float acceleration;                  // Acceleration in steps/second^2
    bool motorDirectionClockwise;        // Motor rotation direction (true = CW, false = CCW)
    
    // Internal methods
    void initializePins();
    void disableMotor();

public:
    // Constructor and destructor
    StepperMotor(int in1, int in2, int in3, int in4, int stepsPerRev = 2048);
    ~StepperMotor();
    
    // Initialization and configuration
    bool begin();
    void setMaxSpeed(float speed);           // Set max speed in steps/second
    void setAcceleration(float accel);       // Set acceleration in steps/second^2
    void setSpeed(float speed);              // Set constant speed for runSpeed()
    void setMotorDirection(bool clockwise);  // Set rotation direction (true = CW, false = CCW)
    bool getMotorDirection() const;          // Get current rotation direction
    
    // Movement methods (blocking)
    void stepClockwise(int steps);
    void stepCounterClockwise(int steps);
    void rotateClockwise(float revolutions);
    void rotateCounterClockwise(float revolutions);
    void moveToPosition(long targetSteps);
    
    // Movement methods (non-blocking)
    void moveToPositionAsync(long targetSteps);
    bool runToPosition();                    // Run until target reached
    bool runSpeed();                         // Run at constant speed
    void run();                              // Call in loop for non-blocking operation
    
    // Position management
    long getCurrentPosition() const;
    void resetPosition();
    void setCurrentPosition(long position);
    long getTargetPosition() const;
    long distanceToGo() const;
    bool isRunning() const;
    
    // Utility methods
    void stop();
    bool isReady() const;
    void printStatus() const;
    
    // Performance optimization methods
    void enableHighPerformanceMode();       // Optimize for maximum speed/torque
    void enablePowerSavingMode();           // Optimize for power efficiency
    
    // Generic utility methods
    void performFullRevolution();
};

#endif // STEPPER_MOTOR_H
