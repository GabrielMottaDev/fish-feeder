#ifndef RGB_LED_H
#define RGB_LED_H

#include <Arduino.h>

/**
 * RGB LED Controller Module
 * 
 * Controls a 4-pin RGB LED (common cathode or common anode) using PWM.
 * Supports color control, intensity adjustment, and timed operations.
 * 
 * Hardware Requirements:
 * - 4-pin RGB LED (common cathode or common anode)
 * - 3x 330Ω resistors (one per color channel)
 * - ESP32 GPIO pins with PWM capability
 * 
 * Features:
 * - Turn LED on/off
 * - Set custom RGB colors (0-255 per channel)
 * - Predefined color constants
 * - Adjust brightness (0-100%)
 * - Timed on/off operations (non-blocking)
 * - Smooth fade effects
 */

class RGBLed {
public:
    /**
     * LED Type Configuration
     */
    enum LEDType {
        COMMON_CATHODE,  // Common cathode (LED on when pin HIGH)
        COMMON_ANODE     // Common anode (LED on when pin LOW)
    };

    /**
     * Device Status States
     * Controls automatic LED behavior based on device state
     */
    enum DeviceStatus {
        STATUS_BOOTING,        // Red 50% blinking 500ms
        STATUS_WIFI_CONNECTING, // Blue 50% blinking 500ms
        STATUS_TIME_SYNCING,   // Yellow 50% blinking 500ms
        STATUS_READY,          // Green 60% static
        STATUS_FEEDING,        // Green 60% blinking 250ms
        STATUS_MANUAL          // Manual control (no automatic status)
    };

    /**
     * Predefined RGB Color Constants
     */
    struct Color {
        uint8_t r, g, b;
    };

    // Predefined colors
    static const Color RED;
    static const Color GREEN;
    static const Color BLUE;
    static const Color YELLOW;
    static const Color CYAN;
    static const Color MAGENTA;
    static const Color WHITE;
    static const Color ORANGE;
    static const Color PURPLE;
    static const Color OFF;

    /**
     * Constructor
     * 
     * @param redPin: GPIO pin for red channel
     * @param greenPin: GPIO pin for green channel
     * @param bluePin: GPIO pin for blue channel
     * @param type: LED type (COMMON_CATHODE or COMMON_ANODE)
     */
    RGBLed(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, LEDType type = COMMON_CATHODE);

    /**
     * Initialize the RGB LED
     * 
     * Sets up PWM channels and initializes pins.
     * Must be called in setup() before using the LED.
     * 
     * @return: true if initialization successful, false otherwise
     */
    bool begin();

    /**
     * Turn LED on with current color
     */
    void turnOn();

    /**
     * Turn LED off (all channels to 0)
     */
    void turnOff();

    /**
     * Check if LED is currently on
     * 
     * @return: true if LED is on, false if off
     */
    bool isOn() const;

    /**
     * Set RGB color (0-255 per channel)
     * 
     * @param red: Red intensity (0-255)
     * @param green: Green intensity (0-255)
     * @param blue: Blue intensity (0-255)
     */
    void setColor(uint8_t red, uint8_t green, uint8_t blue);

    /**
     * Set color using Color struct
     * 
     * @param color: Predefined Color structure
     */
    void setColor(const Color& color);

    /**
     * Get current color
     * 
     * @return: Current Color struct
     */
    Color getColor() const;

    /**
     * Set brightness level (0-100%)
     * 
     * Applies a brightness multiplier to the current color.
     * 
     * @param brightness: Brightness percentage (0-100)
     */
    void setBrightness(uint8_t brightness);

    /**
     * Get current brightness level
     * 
     * @return: Brightness percentage (0-100)
     */
    uint8_t getBrightness() const;

    /**
     * Turn LED on for a specified duration (non-blocking)
     * 
     * LED will automatically turn off after the duration.
     * Must call update() regularly in loop() for timed operations.
     * 
     * @param durationMs: Duration in milliseconds
     */
    void turnOnFor(unsigned long durationMs);

    /**
     * Turn LED on for duration with specific color (non-blocking)
     * 
     * @param durationMs: Duration in milliseconds
     * @param color: Color to display
     */
    void turnOnFor(unsigned long durationMs, const Color& color);

    /**
     * Fade to a new color over specified duration (non-blocking)
     * 
     * @param targetColor: Target color to fade to
     * @param durationMs: Fade duration in milliseconds
     */
    void fadeTo(const Color& targetColor, unsigned long durationMs);

    /**
     * Blink LED with specified interval (non-blocking)
     * 
     * @param intervalMs: Blink interval in milliseconds
     * @param count: Number of blinks (0 = infinite)
     */
    void blink(unsigned long intervalMs, uint16_t count = 0);

    /**
     * Stop blinking
     */
    void stopBlink();

    /**
     * Set device status (automatic LED behavior)
     * 
     * Controls LED color, brightness, and blinking pattern based on device state.
     * 
     * @param status: Device status enum
     */
    void setDeviceStatus(DeviceStatus status);

    /**
     * Get current device status
     * 
     * @return: Current DeviceStatus enum
     */
    DeviceStatus getDeviceStatus() const;

    /**
     * Update LED state (call regularly in loop)
     * 
     * Handles timed operations, fading, and blinking.
     * Must be called frequently for non-blocking operations.
     */
    void update();

    /**
     * Get status string for debugging
     * 
     * @return: String with LED status information
     */
    String getStatus() const;

private:
    // Pin assignments
    uint8_t _redPin;
    uint8_t _greenPin;
    uint8_t _bluePin;

    // PWM channels (ESP32 has 16 PWM channels: 0-15)
    uint8_t _redChannel;
    uint8_t _greenChannel;
    uint8_t _blueChannel;

    // LED type
    LEDType _ledType;

    // Current color state
    Color _currentColor;
    uint8_t _brightness;  // 0-100%
    bool _isOn;

    // Timed operation state
    bool _timedOperation;
    unsigned long _timedStartTime;
    unsigned long _timedDuration;

    // Fade effect state
    bool _fadeInProgress;
    Color _fadeStartColor;
    Color _fadeTargetColor;
    unsigned long _fadeStartTime;
    unsigned long _fadeDuration;

    // Blink state
    bool _blinkActive;
    unsigned long _blinkInterval;
    unsigned long _blinkLastChange;
    uint16_t _blinkTotalCount;       // Total number of complete blinks requested
    uint16_t _blinkCompletedCount;   // Number of complete blinks done
    bool _blinkCurrentlyOn;          // Current on/off state during blinking

    // Device status state
    DeviceStatus _deviceStatus;

    // PWM configuration
    static const uint32_t PWM_FREQUENCY = 5000;  // 5 kHz
    static const uint8_t PWM_RESOLUTION = 8;     // 8-bit (0-255)

    /**
     * Apply current color to hardware with brightness adjustment
     */
    void applyColor();

    /**
     * Write PWM value to channel (handles common anode/cathode)
     * 
     * @param channel: PWM channel
     * @param value: PWM value (0-255)
     */
    void writePWM(uint8_t channel, uint8_t value);

    /**
     * Calculate brightness-adjusted color value
     * 
     * @param value: Original color value (0-255)
     * @return: Brightness-adjusted value (0-255)
     */
    uint8_t applyBrightness(uint8_t value) const;

    /**
     * Internal turn on (for blink state machine)
     * Does NOT cancel active blink operation
     */
    void _turnOn();

    /**
     * Internal turn off (for blink state machine)
     * Does NOT cancel active blink operation
     */
    void _turnOff();
};

#endif // RGB_LED_H
