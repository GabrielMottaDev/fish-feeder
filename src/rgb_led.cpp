#include "rgb_led.h"

// Static PWM channel counter to avoid conflicts
static uint8_t nextPWMChannel = 0;

// Predefined color constants
const RGBLed::Color RGBLed::RED = {255, 0, 0};
const RGBLed::Color RGBLed::GREEN = {0, 255, 0};
const RGBLed::Color RGBLed::BLUE = {0, 0, 255};
const RGBLed::Color RGBLed::YELLOW = {255, 80, 0};
const RGBLed::Color RGBLed::CYAN = {0, 255, 255};
const RGBLed::Color RGBLed::MAGENTA = {255, 0, 255};
const RGBLed::Color RGBLed::WHITE = {255, 255, 255};
const RGBLed::Color RGBLed::ORANGE = {255, 20, 0};
const RGBLed::Color RGBLed::PURPLE = {128, 0, 128};
const RGBLed::Color RGBLed::OFF = {0, 0, 0};

RGBLed::RGBLed(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, LEDType type)
    : _redPin(redPin),
      _greenPin(greenPin),
      _bluePin(bluePin),
      _ledType(type),
      _currentColor({0, 0, 0}),
      _brightness(100),
      _isOn(false),
      _timedOperation(false),
      _timedStartTime(0),
      _timedDuration(0),
      _fadeInProgress(false),
      _fadeStartColor({0, 0, 0}),
      _fadeTargetColor({0, 0, 0}),
      _fadeStartTime(0),
      _fadeDuration(0),
      _blinkActive(false),
      _blinkInterval(0),
      _blinkLastChange(0),
      _blinkTotalCount(0),
      _blinkCompletedCount(0),
      _blinkCurrentlyOn(false),
      _deviceStatus(STATUS_MANUAL) {
    
    // Assign PWM channels (ensure we don't exceed 16 channels)
    _redChannel = nextPWMChannel++;
    _greenChannel = nextPWMChannel++;
    _blueChannel = nextPWMChannel++;
    
    if (nextPWMChannel > 15) {
        nextPWMChannel = 0;  // Wrap around if needed
    }
}

bool RGBLed::begin() {
    // Configure PWM channels
    ledcSetup(_redChannel, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(_greenChannel, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(_blueChannel, PWM_FREQUENCY, PWM_RESOLUTION);
    
    // Attach pins to PWM channels
    ledcAttachPin(_redPin, _redChannel);
    ledcAttachPin(_greenPin, _greenChannel);
    ledcAttachPin(_bluePin, _blueChannel);
    
    // Initialize LED to off state
    turnOff();
    
    return true;
}

void RGBLed::turnOn() {
    // Stop any active blink when manually turning on
    if (_blinkActive) {
        _blinkActive = false;
        _blinkCompletedCount = 0;
        _blinkTotalCount = 0;
    }
    
    _isOn = true;
    applyColor();
}

void RGBLed::turnOff() {
    _isOn = false;
    _timedOperation = false;
    _fadeInProgress = false;
    // DO NOT stop blink here - let blink control itself
    // _blinkActive = false;  // REMOVED - was causing blink to stop
    
    // Turn off all channels
    writePWM(_redChannel, 0);
    writePWM(_greenChannel, 0);
    writePWM(_blueChannel, 0);
}

void RGBLed::_turnOn() {
    // Internal turn on for blink state machine
    // Does NOT cancel active blink
    _isOn = true;
    applyColor();
}

void RGBLed::_turnOff() {
    // Internal turn off for blink state machine
    // Does NOT cancel active blink or timed operations
    _isOn = false;
    applyColor();
}

bool RGBLed::isOn() const {
    return _isOn;
}

void RGBLed::setColor(uint8_t red, uint8_t green, uint8_t blue) {
    // Stop any active blink when manually changing color
    if (_blinkActive) {
        _blinkActive = false;
        _blinkCompletedCount = 0;
        _blinkTotalCount = 0;
    }
    
    _currentColor.r = red;
    _currentColor.g = green;
    _currentColor.b = blue;
    
    if (_isOn) {
        applyColor();
    }
}

void RGBLed::setColor(const Color& color) {
    setColor(color.r, color.g, color.b);
}

RGBLed::Color RGBLed::getColor() const {
    return _currentColor;
}

void RGBLed::setBrightness(uint8_t brightness) {
    _brightness = constrain(brightness, 0, 100);
    
    if (_isOn) {
        applyColor();
    }
}

uint8_t RGBLed::getBrightness() const {
    return _brightness;
}

void RGBLed::turnOnFor(unsigned long durationMs) {
    // Stop any active blink when starting timed operation
    if (_blinkActive) {
        _blinkActive = false;
        _blinkCompletedCount = 0;
        _blinkTotalCount = 0;
    }
    
    _timedOperation = true;
    _timedStartTime = millis();
    _timedDuration = durationMs;
    turnOn();
}

void RGBLed::turnOnFor(unsigned long durationMs, const Color& color) {
    setColor(color);
    turnOnFor(durationMs);
}

void RGBLed::fadeTo(const Color& targetColor, unsigned long durationMs) {
    // Stop any active blink when starting fade
    if (_blinkActive) {
        _blinkActive = false;
        _blinkCompletedCount = 0;
        _blinkTotalCount = 0;
    }
    
    _fadeInProgress = true;
    _fadeStartColor = _currentColor;
    _fadeTargetColor = targetColor;
    _fadeStartTime = millis();
    _fadeDuration = durationMs;
    
    if (!_isOn) {
        turnOn();
    }
}

void RGBLed::blink(unsigned long intervalMs, uint16_t count) {
    // CRITICAL: Cancel any existing blink first
    if (_blinkActive) {
        _blinkActive = false;
        _blinkCompletedCount = 0;
        _blinkTotalCount = 0;
    }
    
    // DO NOT change color - use whatever color is already set
    // If no color is set, _currentColor will be from last operation
    
    // Initialize NEW blink state
    _blinkActive = true;
    _blinkInterval = intervalMs;
    _blinkTotalCount = count;           // 0 = infinite, >0 = specific count
    _blinkCompletedCount = 0;           // Start from zero
    _blinkCurrentlyOn = false;          // Will turn on in first update
    _blinkLastChange = millis();        // Mark start time
    
    // Turn LED off initially so first toggle turns it on
    _turnOff();  // Use internal method - preserves blink state
}

void RGBLed::stopBlink() {
    _blinkActive = false;
    _blinkCompletedCount = 0;
    _blinkTotalCount = 0;
    turnOff();  // Final turn off
}

/**
 * Set device status and apply corresponding LED pattern
 */
void RGBLed::setDeviceStatus(DeviceStatus status) {
    _deviceStatus = status;
    
    // Stop any manual operations when entering automatic status mode
    if (status != STATUS_MANUAL) {
        _timedOperation = false;
        _fadeInProgress = false;
    }
    
    switch (status) {
        case STATUS_BOOTING:
            // Red 50% blinking 500ms
            setColor(RED);
            setBrightness(50);
            blink(500, 0);  // Infinite blink
            break;
            
        case STATUS_WIFI_CONNECTING:
            // Blue 50% blinking 500ms
            setColor(BLUE);
            setBrightness(50);
            blink(500, 0);  // Infinite blink
            break;
            
        case STATUS_TIME_SYNCING:
            // Yellow 50% blinking 500ms
            setColor(YELLOW);
            setBrightness(50);
            blink(500, 0);  // Infinite blink
            break;
            
        case STATUS_READY:
            // Green 60% static
            stopBlink();  // Stop any blinking
            setColor(GREEN);
            setBrightness(60);
            turnOn();
            break;
            
        case STATUS_FEEDING:
            // Green 60% blinking 250ms
            setColor(GREEN);
            setBrightness(60);
            blink(250, 0);  // Infinite blink
            break;
            
        case STATUS_MANUAL:
            // Manual control - do nothing, user controls LED
            stopBlink();
            break;
    }
}

/**
 * Get current device status
 */
RGBLed::DeviceStatus RGBLed::getDeviceStatus() const {
    return _deviceStatus;
}

void RGBLed::update() {
    // Handle timed operation
    if (_timedOperation) {
        if (millis() - _timedStartTime >= _timedDuration) {
            turnOff();
        }
    }
    
    // Handle fade effect
    if (_fadeInProgress) {
        unsigned long elapsed = millis() - _fadeStartTime;
        
        if (elapsed >= _fadeDuration) {
            // Fade complete
            setColor(_fadeTargetColor);
            _fadeInProgress = false;
        } else {
            // Calculate interpolated color
            float progress = (float)elapsed / (float)_fadeDuration;
            
            uint8_t r = _fadeStartColor.r + ((_fadeTargetColor.r - _fadeStartColor.r) * progress);
            uint8_t g = _fadeStartColor.g + ((_fadeTargetColor.g - _fadeStartColor.g) * progress);
            uint8_t b = _fadeStartColor.b + ((_fadeTargetColor.b - _fadeStartColor.b) * progress);
            
            setColor(r, g, b);
        }
    }
    
    // Handle blinking - COMPLETELY INDEPENDENT LOGIC
    if (_blinkActive) {
        unsigned long currentTime = millis();
        
        // Check if it's time to change state
        if (currentTime - _blinkLastChange >= _blinkInterval) {
            _blinkLastChange = currentTime;
            
            // Toggle LED state
            if (_blinkCurrentlyOn) {
                // LED is ON, turn it OFF
                _turnOff();  // Use internal method - does NOT cancel blink
                _blinkCurrentlyOn = false;
                
                // One complete blink cycle finished (ON → OFF)
                _blinkCompletedCount++;
                
                // Check if we've completed all requested blinks
                if (_blinkTotalCount > 0 && _blinkCompletedCount >= _blinkTotalCount) {
                    _blinkActive = false;
                }
            } else {
                // LED is OFF, turn it ON
                _turnOn();  // Use internal method - does NOT cancel blink
                _blinkCurrentlyOn = true;
            }
        }
    }
}

String RGBLed::getStatus() const {
    String status = "RGB LED Status:\n";
    status += "  State: " + String(_isOn ? "ON" : "OFF") + "\n";
    status += "  Color: R=" + String(_currentColor.r) + 
              " G=" + String(_currentColor.g) + 
              " B=" + String(_currentColor.b) + "\n";
    status += "  Brightness: " + String(_brightness) + "%\n";
    status += "  Pins: R=" + String(_redPin) + 
              " G=" + String(_greenPin) + 
              " B=" + String(_bluePin) + "\n";
    status += "  Type: " + String(_ledType == COMMON_CATHODE ? "Common Cathode" : "Common Anode") + "\n";
    
    if (_timedOperation) {
        unsigned long remaining = _timedDuration - (millis() - _timedStartTime);
        status += "  Timed: " + String(remaining) + "ms remaining\n";
    }
    
    if (_fadeInProgress) {
        unsigned long remaining = _fadeDuration - (millis() - _fadeStartTime);
        status += "  Fading: " + String(remaining) + "ms remaining\n";
    }
    
    if (_blinkActive) {
        status += "  Blinking: Interval=" + String(_blinkInterval) + "ms";
        status += ", Completed=" + String(_blinkCompletedCount);
        if (_blinkTotalCount > 0) {
            status += "/" + String(_blinkTotalCount);
        } else {
            status += " (Infinite)";
        }
        status += ", Currently=" + String(_blinkCurrentlyOn ? "ON" : "OFF");
        status += "\n";
    }
    
    return status;
}

void RGBLed::applyColor() {
    if (!_isOn) {
        // If LED is off, write 0 to all channels
        writePWM(_redChannel, 0);
        writePWM(_greenChannel, 0);
        writePWM(_blueChannel, 0);
        return;
    }
    
    // LED is on, apply color with brightness
    uint8_t r = applyBrightness(_currentColor.r);
    uint8_t g = applyBrightness(_currentColor.g);
    uint8_t b = applyBrightness(_currentColor.b);
    
    writePWM(_redChannel, r);
    writePWM(_greenChannel, g);
    writePWM(_blueChannel, b);
}

void RGBLed::writePWM(uint8_t channel, uint8_t value) {
    if (_ledType == COMMON_ANODE) {
        // Invert for common anode (LED on when pin LOW)
        value = 255 - value;
    }
    
    ledcWrite(channel, value);
}

uint8_t RGBLed::applyBrightness(uint8_t value) const {
    return (value * _brightness) / 100;
}
