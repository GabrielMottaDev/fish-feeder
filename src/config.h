#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/**
 * Global Configuration File
 * 
 * This file contains all configurable parameters for the Fish Feeder system.
 * These constants can be easily modified to adjust system behavior without
 * changing the core logic in multiple files.
 */

// ============================================================================
// FEEDING CONFIGURATION
// ============================================================================

/**
 * Feeding Parameters
 * 
 * These values control how much food is dispensed per portion and
 * the valid range of portions that can be requested.
 */

// Rotation per food portion (as fraction of full revolution)
// Default: 1/8 revolution = 0.125 (256 steps for 2048 steps/revolution)
extern const float FOOD_PORTION_ROTATION;

// Minimum number of portions allowed per feeding
extern const int MIN_FOOD_PORTIONS;

// Maximum number of portions allowed per feeding  
extern const int MAX_FOOD_PORTIONS;

// Steps per full revolution for 28BYJ-48 stepper motor
extern const int STEPS_PER_REVOLUTION;

// ============================================================================
// MOTOR CONFIGURATION
// ============================================================================

/**
 * Stepper Motor Default Settings
 * 
 * These values define the default speed and acceleration for the motor.
 * Can be overridden at runtime but provide good starting defaults.
 */

// Default maximum speed in steps per second
extern const float DEFAULT_MAX_SPEED;

// Default acceleration in steps per second squared
extern const float DEFAULT_ACCELERATION;

/**
 * Vibration Motor Configuration
 * 
 * Settings for the vibracall motor (1027 3V) controlled via PWM through
 * NPN 2N2222 transistor circuit with 1kΩ resistor, 1N4007 diode, and 100nF capacitor.
 */

// GPIO pin for vibration motor PWM control
extern const uint8_t VIBRATION_MOTOR_PIN;

// PWM channel for vibration motor (0-15 on ESP32)
extern const uint8_t VIBRATION_PWM_CHANNEL;

// PWM frequency in Hz (1000Hz is good for motor control)
extern const uint32_t VIBRATION_PWM_FREQUENCY;

// PWM resolution in bits (8 bits = 0-255 duty cycle range)
extern const uint8_t VIBRATION_PWM_RESOLUTION;

// Vibration motor maintenance task interval (milliseconds)
extern const unsigned long VIBRATION_MAINTENANCE_INTERVAL;

/**
 * RGB LED Configuration
 * 
 * Settings for 4-pin RGB LED (common cathode or common anode) with PWM control.
 * Used for visual status indication and user feedback.
 * Hardware: 4-pin RGB LED + 3x 330Ω resistors
 */

// GPIO pins for RGB LED channels
extern const uint8_t RGB_LED_RED_PIN;
extern const uint8_t RGB_LED_GREEN_PIN;
extern const uint8_t RGB_LED_BLUE_PIN;

// RGB LED type (0 = common cathode, 1 = common anode)
extern const uint8_t RGB_LED_TYPE;

// RGB LED maintenance task interval (milliseconds)
extern const unsigned long RGB_LED_MAINTENANCE_INTERVAL;

/**
 * Touch Sensor Configuration (TTP223)
 * 
 * Settings for TTP223 capacitive touch sensor module.
 * Used for manual user interaction and emergency controls.
 * Hardware: TTP223 module connected to GPIO pin
 */

// GPIO pin for touch sensor input
extern const uint8_t TOUCH_SENSOR_PIN;

// Touch sensor active logic (false = active HIGH, true = active LOW)
extern const bool TOUCH_SENSOR_ACTIVE_LOW;

// Touch sensor debounce delay (milliseconds)
extern const unsigned long TOUCH_SENSOR_DEBOUNCE_DELAY;

// Touch sensor long press duration (milliseconds)
extern const unsigned long TOUCH_SENSOR_LONG_PRESS_DURATION;

// Touch sensor maintenance task interval (milliseconds)
extern const unsigned long TOUCH_SENSOR_MAINTENANCE_INTERVAL;

// ============================================================================
// SERIAL COMMUNICATION CONFIGURATION
// ============================================================================

/**
 * Serial Communication Settings
 */

// Serial baud rate for communication
extern const long SERIAL_BAUD_RATE;

// ============================================================================
// TASK SCHEDULER CONFIGURATION
// ============================================================================

/**
 * Task Scheduler Timing Settings
 * 
 * These values control the frequency of various system tasks.
 * Adjust these to optimize system performance and responsiveness.
 */

// Time display task interval (milliseconds)
extern const unsigned long DISPLAY_TIME_INTERVAL;

// Serial processing task interval (milliseconds) 
extern const unsigned long SERIAL_PROCESS_INTERVAL;

// Motor maintenance task interval (milliseconds)
extern const unsigned long MOTOR_MAINTENANCE_INTERVAL;

// Serial connection timeout (milliseconds)
extern const unsigned long SERIAL_TIMEOUT;

// ============================================================================
// WIFI & BLUETOOTH CONFIGURATION
// ============================================================================

/**
 * WiFi and Bluetooth Settings
 * 
 * These values control wireless connectivity behavior and timeouts.
 */

// WiFi connection timeout (milliseconds)
extern const unsigned long WIFI_CONNECTION_TIMEOUT;

// WiFi auto-reconnection interval (milliseconds)
extern const unsigned long WIFI_RECONNECT_INTERVAL;

// Maximum number of saved WiFi networks
extern const int MAX_SAVED_NETWORKS;

// Default Bluetooth device name
extern const char* DEFAULT_BLUETOOTH_NAME;

// WiFi scan timeout (milliseconds)
extern const unsigned long WIFI_SCAN_TIMEOUT;

// ============================================================================
// WIFI PORTAL CONFIGURATION
// ============================================================================

/**
 * WiFi Configuration Portal Settings
 * 
 * These values control the WiFi configuration portal behavior including
 * automatic startup, timeout settings, and access credentials.
 */

// Enable WiFi portal auto-start on boot
extern const bool WIFI_PORTAL_AUTO_START;

// Enable WiFi portal on connection loss
extern const bool WIFI_PORTAL_ON_DISCONNECT;

// WiFi portal timeout (milliseconds) - 15 minutes
extern const unsigned long WIFI_PORTAL_TIMEOUT;

// Default WiFi portal access point name
extern const char* WIFI_PORTAL_AP_NAME;

// WiFi portal access password (empty = no password)
extern const char* WIFI_PORTAL_AP_PASSWORD;

// WiFi portal connection check interval (milliseconds)
extern const unsigned long WIFI_CONNECTION_CHECK_INTERVAL;

// ============================================================================
// NTP TIME SYNCHRONIZATION CONFIGURATION
// ============================================================================

/**
 * NTP Time Synchronization Settings
 * 
 * These values control automatic time synchronization with internet time servers
 * to keep the RTC module accurate.
 */

// NTP synchronization interval (milliseconds) - 30 minutes
extern const unsigned long NTP_SYNC_INTERVAL;

/**
 * Time Server Entry Structure
 * 
 * Defines a time server that can be either NTP (UDP) or HTTP-based.
 * This allows intercalating between different protocols for better reliability.
 */
struct TimeServerEntry {
    const char* type;      // "ntp" or "http"
    const char* server;    // Server address or URL
};

// Intercalated time servers array (NTP and HTTP mixed for better reliability)
extern const TimeServerEntry TIME_SERVERS[];
extern const int TIME_SERVERS_COUNT;

// DNS server addresses array (priority order: index 0 = highest priority)
extern const char* DNS_SERVERS[];
extern const int DNS_SERVERS_COUNT;

// GMT offset in seconds (Brazil Standard Time: UTC-3 = -3 * 3600)
extern const long GMT_OFFSET_SEC;

// Daylight saving time offset in seconds (Brazil doesn't use DST consistently)
extern const int DAYLIGHT_OFFSET_SEC;

// NTP timeout for synchronization attempts (milliseconds)
extern const unsigned long NTP_SYNC_TIMEOUT;

// HTTP time fallback timeout (milliseconds)
extern const unsigned long HTTP_TIME_TIMEOUT;

// Delay after WiFi connection before first NTP sync (milliseconds)
extern const unsigned long NTP_INITIAL_SYNC_DELAY;

// NVRAM key for storing last NTP sync timestamp (Unix time in seconds)
extern const char* NTP_LAST_SYNC_NVRAM_KEY;

// ============================================================================
// FEEDING SCHEDULE CONFIGURATION
// ============================================================================

/**
 * Feeding Schedule System Settings
 * 
 * These values control the automatic feeding schedule system including
 * tolerance for missed feedings, recovery periods, and task intervals.
 */

// Schedule monitoring task interval (milliseconds) - 30 seconds
extern const unsigned long FEEDING_SCHEDULE_MONITOR_INTERVAL;

// Tolerance for missed feedings (minutes) - allow up to 30 minutes late
extern const uint16_t FEEDING_SCHEDULE_TOLERANCE_MINUTES;

// Maximum recovery period after power loss (hours) - look back up to 12 hours
extern const uint16_t FEEDING_SCHEDULE_MAX_RECOVERY_HOURS;

// Maximum number of scheduled feedings
extern const uint8_t MAX_SCHEDULED_FEEDINGS;

/**
 * Feeding Schedule Structure
 * 
 * Structure to define a scheduled feeding time with all necessary parameters.
 * Defined here to avoid circular includes between config.h and feeding_schedule.h
 */
struct ScheduledFeeding {
    uint8_t hour;           // 0-23
    uint8_t minute;         // 0-59
    uint8_t second;         // 0-59 (optional, default 0)
    uint8_t portions;       // Number of portions to feed
    bool enabled;           // Whether this schedule is active
    char description[50];   // Description for this feeding (mutable array for NVRAM persistence)
};

/**
 * Default Feeding Schedule Configuration
 * 
 * These define the default feeding times. Users can modify these or 
 * replace with custom schedules at runtime.
 */

// Default feeding schedule array
extern ScheduledFeeding DEFAULT_FEEDING_SCHEDULE[];
extern const uint8_t DEFAULT_SCHEDULE_COUNT;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Convert portions to motor steps
 * 
 * @param portions: Number of food portions
 * @return: Number of steps needed
 */
int portionsToSteps(int portions);

/**
 * Validate if portion count is within allowed range
 * 
 * @param portions: Number of portions to validate
 * @return: true if valid, false otherwise
 */
bool isValidPortionCount(int portions);

/**
 * Clamp portion count to valid range
 * 
 * @param portions: Requested portions
 * @return: Clamped value within MIN_FOOD_PORTIONS to MAX_FOOD_PORTIONS
 */
int clampPortions(int portions);

#endif // CONFIG_H