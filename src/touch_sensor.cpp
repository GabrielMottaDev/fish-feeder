#include "touch_sensor.h"
#include "console_manager.h"

/**
 * Touch Sensor Module Implementation (TTP223)
 * 
 * Non-blocking capacitive touch sensor controller with debouncing and callbacks.
 */

/**
 * Constructor
 * 
 * @param pin: GPIO pin connected to TTP223 output
 * @param activeLow: true if module outputs LOW when touched (rare), false if HIGH (default)
 */
TouchSensor::TouchSensor(uint8_t pin, bool activeLow)
    : _pin(pin),
      _activeLow(activeLow),
      _touched(false),
      _lastRawState(false),
      _lastChangeTime(0),
      _touchStartTime(0),
      _debounceDelay(50),        // 50ms default debounce
      _pendingState(false),
      _longPressEnabled(true),
      _longPressDuration(1000),  // 1 second default long press
      _longPressDetected(false),
      _callback(nullptr),
      _touchCount(0),
      _longPressCount(0) {
}

/**
 * Initialize the touch sensor
 * 
 * @param usePullUp: true to enable internal pull-up resistor (default: false)
 * @return: true if initialization successful, false otherwise
 */
bool TouchSensor::begin(bool usePullUp) {
    // Configure pin mode
    if (usePullUp) {
        pinMode(_pin, INPUT_PULLUP);
        Console::print(F("Touch sensor initialized with pull-up on pin "));
        Console::println(String(_pin));
    } else {
        pinMode(_pin, INPUT);
        Console::print(F("Touch sensor initialized on pin "));
        Console::println(String(_pin));
    }

    // Read initial state
    _lastRawState = readRaw();
    _pendingState = _lastRawState;
    _touched = _lastRawState;
    _lastChangeTime = millis();

    Console::print(F("Touch sensor ready (active "));
    Console::print(_activeLow ? F("LOW") : F("HIGH"));
    Console::println(F(")"));
    return true;
}

/**
 * Update touch sensor state (call regularly in loop)
 * 
 * Handles touch detection, debouncing, and callback invocation.
 * CRITICAL: Non-blocking - uses millis() timing only
 */
void TouchSensor::update() {
    // Read current raw state
    bool currentRawState = readRaw();
    unsigned long currentTime = millis();

    // Check for state change
    if (currentRawState != _lastRawState) {
        // State changed - start debounce timer
        _lastChangeTime = currentTime;
        _lastRawState = currentRawState;
        _pendingState = currentRawState;
    }

    // Check if state has been stable for debounce period
    if (currentTime - _lastChangeTime >= _debounceDelay) {
        // State is stable - check if it's different from current debounced state
        if (_pendingState != _touched) {
            // Debounced state change confirmed
            _touched = _pendingState;

            if (_touched) {
                // Touch pressed
                _touchStartTime = currentTime;
                _longPressDetected = false;
                _touchCount++;
                
                // Invoke callback
                invokeCallback(TOUCH_PRESSED, 0);
                
                Console::print(F("Touch detected (count: "));
                Console::print(String(_touchCount));
                Console::println(F(")"));
            } else {
                // Touch released
                unsigned long touchDuration = currentTime - _touchStartTime;
                
                // Invoke callback
                invokeCallback(TOUCH_RELEASED, touchDuration);
                
                Console::print(F("Touch released (duration: "));
                Console::print(String(touchDuration));
                Console::println(F("ms)"));
            }
        }
    }

    // Check for long press (only if touch is active and long press not yet detected)
    if (_touched && _longPressEnabled && !_longPressDetected) {
        unsigned long touchDuration = currentTime - _touchStartTime;
        
        if (touchDuration >= _longPressDuration) {
            // Long press detected
            _longPressDetected = true;
            _longPressCount++;
            
            // Invoke callback
            invokeCallback(TOUCH_LONG_PRESS, touchDuration);
            
            Console::print(F("Long press detected (duration: "));
            Console::print(String(touchDuration));
            Console::print(F("ms, count: "));
            Console::print(String(_longPressCount));
            Console::println(F(")"));
        }
    }
}

/**
 * Check if sensor is currently touched (debounced state)
 * 
 * @return: true if touched, false if not touched
 */
bool TouchSensor::isTouched() const {
    return _touched;
}

/**
 * Check if sensor is currently touched (raw state, no debouncing)
 * 
 * @return: true if touched, false if not touched
 */
bool TouchSensor::isTouchedRaw() const {
    return readRaw();
}

/**
 * Get touch duration (milliseconds)
 * 
 * @return: Touch duration in milliseconds (0 if not touched)
 */
unsigned long TouchSensor::getTouchDuration() const {
    if (!_touched) {
        return 0;
    }
    return millis() - _touchStartTime;
}

/**
 * Check if long press is active
 * 
 * @return: true if long press detected, false otherwise
 */
bool TouchSensor::isLongPress() const {
    return _longPressDetected;
}

/**
 * Set callback function for touch events
 * 
 * @param callback: Function to call on touch events (NULL to disable)
 */
void TouchSensor::setCallback(TouchCallback callback) {
    _callback = callback;
    
    if (callback) {
        Console::println(F("Touch sensor callback enabled"));
    } else {
        Console::println(F("Touch sensor callback disabled"));
    }
}

/**
 * Set debounce delay (milliseconds)
 * 
 * @param delayMs: Debounce delay in milliseconds
 */
void TouchSensor::setDebounceDelay(unsigned long delayMs) {
    _debounceDelay = delayMs;
    Console::print(F("Touch sensor debounce delay set to "));
    Console::print(String(delayMs));
    Console::println(F("ms"));
}

/**
 * Set long press duration (milliseconds)
 * 
 * @param durationMs: Long press duration in milliseconds
 */
void TouchSensor::setLongPressDuration(unsigned long durationMs) {
    _longPressDuration = durationMs;
    Console::print(F("Touch sensor long press duration set to "));
    Console::print(String(durationMs));
    Console::println(F("ms"));
}

/**
 * Enable/disable long press detection
 * 
 * @param enabled: true to enable long press detection, false to disable
 */
void TouchSensor::setLongPressEnabled(bool enabled) {
    _longPressEnabled = enabled;
    Console::print(F("Touch sensor long press "));
    Console::println(enabled ? F("enabled") : F("disabled"));
}

/**
 * Get debounce delay
 * 
 * @return: Debounce delay in milliseconds
 */
unsigned long TouchSensor::getDebounceDelay() const {
    return _debounceDelay;
}

/**
 * Get long press duration
 * 
 * @return: Long press duration in milliseconds
 */
unsigned long TouchSensor::getLongPressDuration() const {
    return _longPressDuration;
}

/**
 * Check if long press detection is enabled
 * 
 * @return: true if enabled, false if disabled
 */
bool TouchSensor::isLongPressEnabled() const {
    return _longPressEnabled;
}

/**
 * Get total number of touches detected since initialization
 * 
 * @return: Total touch count
 */
unsigned long TouchSensor::getTouchCount() const {
    return _touchCount;
}

/**
 * Get total number of long presses detected since initialization
 * 
 * @return: Total long press count
 */
unsigned long TouchSensor::getLongPressCount() const {
    return _longPressCount;
}

/**
 * Reset touch statistics
 */
void TouchSensor::resetStatistics() {
    _touchCount = 0;
    _longPressCount = 0;
    Console::println(F("Touch sensor statistics reset"));
}

/**
 * Get status string for debugging
 * 
 * @return: String with sensor status information
 */
String TouchSensor::getStatus() const {
    String status = "";
    status += "Touch Sensor Status:\n";
    status += "  Pin: " + String(_pin) + "\n";
    status += "  Active Logic: ";
    status += _activeLow ? "LOW" : "HIGH";
    status += "\n  Current State: ";
    status += _touched ? "TOUCHED" : "NOT TOUCHED";
    status += "\n  Raw State: ";
    status += readRaw() ? "TOUCHED" : "NOT TOUCHED";
    status += "\n";
    
    if (_touched) {
        unsigned long duration = millis() - _touchStartTime;
        status += "  Touch Duration: " + String(duration) + "ms\n";
        status += "  Long Press: ";
        status += _longPressDetected ? "YES" : "NO";
        status += "\n";
    }
    
    status += "  Debounce Delay: " + String(_debounceDelay) + "ms\n";
    status += "  Long Press Enabled: ";
    status += _longPressEnabled ? "YES" : "NO";
    status += "\n  Long Press Duration: " + String(_longPressDuration) + "ms\n";
    status += "  Total Touches: " + String(_touchCount) + "\n";
    status += "  Total Long Presses: " + String(_longPressCount) + "\n";
    status += "  Callback: ";
    status += _callback ? "ENABLED" : "DISABLED";
    
    return status;
}

/**
 * Read raw sensor state from pin
 * 
 * @return: true if touched, false if not touched
 */
bool TouchSensor::readRaw() const {
    bool pinState = digitalRead(_pin);
    
    // Invert if active low
    if (_activeLow) {
        return !pinState;
    }
    
    return pinState;
}

/**
 * Invoke callback if set
 * 
 * @param event: Touch event type
 * @param duration: Touch duration (0 if not applicable)
 */
void TouchSensor::invokeCallback(TouchEvent event, unsigned long duration) {
    if (_callback) {
        _callback(event, duration);
    }
}
