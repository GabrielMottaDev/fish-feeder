#ifndef VIBRATION_MOTOR_H
#define VIBRATION_MOTOR_H

#include <Arduino.h>

/**
 * VibrationMotor Class
 * 
 * Controls a vibracall motor (1027 3V) via PWM using a transistor circuit
 * (NPN 2N2222, 1kΩ resistor, 1N4007 diode, 100nF capacitor).
 * 
 * Features:
 * - Non-blocking operation using state machine pattern
 * - PWM intensity control (0-100%)
 * - Timed vibration sequences
 * - Task-based architecture compatible with TaskScheduler
 * 
 * Hardware Setup:
 * - GPIO PWM → 1kΩ resistor → Base NPN 2N2222
 * - Collector → Motor → 3V
 * - Emitter → GND
 * - 1N4007 diode across motor (cathode to 3V)
 * - 100nF ceramic capacitor across motor
 */
class VibrationMotor {
private:
    // Pin configuration
    uint8_t pwmPin;
    
    // PWM configuration
    uint8_t pwmChannel;
    uint32_t pwmFrequency;
    uint8_t pwmResolution;
    
    // State management
    bool isVibrating;
    uint8_t currentIntensity;  // 0-100%
    unsigned long vibrationStartTime;
    unsigned long vibrationDuration;
    bool timedVibration;
    
    // Calculate duty cycle from intensity percentage
    uint32_t intensityToDutyCycle(uint8_t intensity);
    
public:
    /**
     * Constructor
     * 
     * @param pin: GPIO pin connected to transistor base (via 1kΩ resistor)
     * @param channel: PWM channel (0-15 on ESP32)
     * @param frequency: PWM frequency in Hz (default: 1000Hz)
     * @param resolution: PWM resolution in bits (default: 8 bits = 0-255)
     */
    VibrationMotor(uint8_t pin, uint8_t channel = 0, uint32_t frequency = 1000, uint8_t resolution = 8);
    
    /**
     * Initialize the vibration motor
     * Sets up PWM channel and configures GPIO
     * 
     * @return: true if initialization successful
     */
    bool begin();
    
    /**
     * Start continuous vibration at specified intensity
     * 
     * @param intensity: Vibration strength (0-100%)
     *                   0 = off, 100 = maximum vibration
     */
    void startContinuous(uint8_t intensity);
    
    /**
     * Start timed vibration at specified intensity
     * Non-blocking - use updateState() to monitor completion
     * 
     * @param intensity: Vibration strength (0-100%)
     * @param durationMs: Vibration duration in milliseconds
     */
    void startTimed(uint8_t intensity, unsigned long durationMs);
    
    /**
     * Stop vibration immediately
     */
    void stop();
    
    /**
     * Update vibration motor state (non-blocking)
     * Call this regularly (e.g., in TaskScheduler task)
     * Handles automatic stop for timed vibrations
     */
    void updateState();
    
    /**
     * Set vibration intensity for current operation
     * 
     * @param intensity: Vibration strength (0-100%)
     */
    void setIntensity(uint8_t intensity);
    
    /**
     * Check if motor is currently vibrating
     * 
     * @return: true if vibrating, false if stopped
     */
    bool getIsVibrating() const;
    
    /**
     * Get current vibration intensity
     * 
     * @return: Current intensity (0-100%)
     */
    uint8_t getIntensity() const;
    
    /**
     * Get remaining vibration time for timed operations
     * 
     * @return: Remaining milliseconds (0 if not timed or completed)
     */
    unsigned long getRemainingTime() const;
    
    /**
     * Vibration pattern: pulse (on-off cycles)
     * Non-blocking - call in loop or task
     * 
     * @param intensity: Vibration strength (0-100%)
     * @param onTimeMs: Time motor is on in each cycle
     * @param offTimeMs: Time motor is off in each cycle
     * @param cycles: Number of on-off cycles (0 = infinite)
     */
    void startPulsePattern(uint8_t intensity, unsigned long onTimeMs, unsigned long offTimeMs, uint16_t cycles = 0);
    
    /**
     * Get status information for debugging
     * 
     * @return: Status string with current state
     */
    String getStatus() const;
};

#endif // VIBRATION_MOTOR_H
