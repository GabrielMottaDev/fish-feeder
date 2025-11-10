#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <Arduino.h>

/**
 * Touch Sensor Module (TTP223)
 * 
 * Controls a TTP223 capacitive touch sensor module with non-blocking debouncing,
 * long press detection, and callback functionality.
 * 
 * Hardware Requirements:
 * - TTP223 Capacitive Touch Sensor Module
 * - ESP32 GPIO pin (any digital input pin)
 * 
 * Module Specifications:
 * - Operating voltage: 2.0V - 5.5V (3.3V or 5V compatible)
 * - Operating current: 1.5µA - 3.0µA (ultra low power)
 * - Output: HIGH when touched, LOW when not touched
 * - Response time: < 60ms (fast touch detection)
 * - Self-calibration: automatic environmental adaptation
 * 
 * Features:
 * - Non-blocking touch detection with debouncing
 * - Long press detection (configurable duration)
 * - Callback functions for touch events
 * - Touch duration tracking
 * - Status reporting
 * - Pull-up/pull-down configuration
 * 
 * Non-blocking Architecture:
 * - NO delay() calls - uses millis() timing
 * - State machine for touch detection
 * - Debounce filtering to prevent false triggers
 * - Safe to call update() continuously in main loop
 */

class TouchSensor {
public:
    /**
     * Touch Events
     * 
     * Events that can trigger callback functions
     */
    enum TouchEvent {
        TOUCH_PRESSED,      // Touch detected (debounced)
        TOUCH_RELEASED,     // Touch released
        TOUCH_LONG_PRESS    // Long press detected (configurable duration)
    };

    /**
     * Callback function type
     * 
     * Function signature: void callback(TouchEvent event, unsigned long duration)
     * - event: Type of touch event
     * - duration: Duration of touch in milliseconds (for RELEASED and LONG_PRESS)
     */
    typedef void (*TouchCallback)(TouchEvent event, unsigned long duration);

    /**
     * Constructor
     * 
     * @param pin: GPIO pin connected to TTP223 output
     * @param activeLow: true if module outputs LOW when touched (rare), false if HIGH (default)
     */
    TouchSensor(uint8_t pin, bool activeLow = false);

    /**
     * Initialize the touch sensor
     * 
     * Sets up the GPIO pin with pull-up/pull-down resistor.
     * Must be called in setup() before using the sensor.
     * 
     * @param usePullUp: true to enable internal pull-up resistor (default: false)
     * @return: true if initialization successful, false otherwise
     */
    bool begin(bool usePullUp = false);

    /**
     * Update touch sensor state (call regularly in loop)
     * 
     * Handles touch detection, debouncing, and callback invocation.
     * Must be called frequently for responsive touch detection.
     * 
     * CRITICAL: This is a non-blocking function - uses millis() timing only
     */
    void update();

    /**
     * Check if sensor is currently touched (debounced state)
     * 
     * @return: true if touched, false if not touched
     */
    bool isTouched() const;

    /**
     * Check if sensor is currently touched (raw state, no debouncing)
     * 
     * @return: true if touched, false if not touched
     */
    bool isTouchedRaw() const;

    /**
     * Get touch duration (milliseconds)
     * 
     * Returns how long the current touch has been held.
     * Returns 0 if not currently touched.
     * 
     * @return: Touch duration in milliseconds
     */
    unsigned long getTouchDuration() const;

    /**
     * Check if long press is active
     * 
     * @return: true if long press detected, false otherwise
     */
    bool isLongPress() const;

    /**
     * Set callback function for touch events
     * 
     * @param callback: Function to call on touch events (NULL to disable)
     */
    void setCallback(TouchCallback callback);

    /**
     * Set debounce delay (milliseconds)
     * 
     * Time required for stable touch detection before triggering event.
     * 
     * @param delayMs: Debounce delay in milliseconds (default: 50ms)
     */
    void setDebounceDelay(unsigned long delayMs);

    /**
     * Set long press duration (milliseconds)
     * 
     * Duration required to trigger long press event.
     * 
     * @param durationMs: Long press duration in milliseconds (default: 1000ms)
     */
    void setLongPressDuration(unsigned long durationMs);

    /**
     * Enable/disable long press detection
     * 
     * @param enabled: true to enable long press detection, false to disable
     */
    void setLongPressEnabled(bool enabled);

    /**
     * Get debounce delay
     * 
     * @return: Debounce delay in milliseconds
     */
    unsigned long getDebounceDelay() const;

    /**
     * Get long press duration
     * 
     * @return: Long press duration in milliseconds
     */
    unsigned long getLongPressDuration() const;

    /**
     * Check if long press detection is enabled
     * 
     * @return: true if enabled, false if disabled
     */
    bool isLongPressEnabled() const;

    /**
     * Get total number of touches detected since initialization
     * 
     * @return: Total touch count
     */
    unsigned long getTouchCount() const;

    /**
     * Get total number of long presses detected since initialization
     * 
     * @return: Total long press count
     */
    unsigned long getLongPressCount() const;

    /**
     * Reset touch statistics
     */
    void resetStatistics();

    /**
     * Get status string for debugging
     * 
     * @return: String with sensor status information
     */
    String getStatus() const;

private:
    // Pin configuration
    uint8_t _pin;
    bool _activeLow;

    // Touch state
    bool _touched;               // Current debounced state
    bool _lastRawState;          // Last raw pin reading
    unsigned long _lastChangeTime;  // Time of last state change
    unsigned long _touchStartTime;  // Time when touch started

    // Debouncing
    unsigned long _debounceDelay;
    bool _pendingState;          // State waiting for debounce confirmation

    // Long press detection
    bool _longPressEnabled;
    unsigned long _longPressDuration;
    bool _longPressDetected;     // Long press already triggered for current touch

    // Callback
    TouchCallback _callback;

    // Statistics
    unsigned long _touchCount;
    unsigned long _longPressCount;

    /**
     * Read raw sensor state from pin
     * 
     * @return: true if touched, false if not touched
     */
    bool readRaw() const;

    /**
     * Invoke callback if set
     * 
     * @param event: Touch event type
     * @param duration: Touch duration (0 if not applicable)
     */
    void invokeCallback(TouchEvent event, unsigned long duration = 0);
};

#endif // TOUCH_SENSOR_H
