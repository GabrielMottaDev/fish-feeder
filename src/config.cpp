#include "config.h"

// ============================================================================
// FEEDING CONFIGURATION VALUES
// ============================================================================

// Rotation per food portion (1/8 revolution = 12.5% of full rotation)
// FEEDING PORTION ROTATION OPTIONS:
//   • 1/16 revolution (0.0625) = 128 steps = very small portion
//   • 1/12 revolution (0.0833) = 171 steps = small portion
//   • 1/8 revolution (0.125) = 256 steps = standard portion (CURRENT)
//   • 1/6 revolution (0.167) = 341 steps = medium portion
//   • 1/4 revolution (0.25) = 512 steps = large portion
//   • 1/3 revolution (0.333) = 682 steps = very large portion
//   • RECOMMENDED: 0.125 (1/8 revolution) for consistent aquarium feeding
const float FOOD_PORTION_ROTATION = 1.0f;

// Minimum and maximum portions allowed per feeding
const int MIN_FOOD_PORTIONS = 1;
const int MAX_FOOD_PORTIONS = 20;

// Steps per revolution for 28BYJ-48 stepper motor in half-step mode
// 28BYJ-48 RESOLUTION SPECIFICATIONS:
//   • FULL-STEP MODE: 2048 steps/revolution (higher torque, lower precision)
//   • HALF-STEP MODE: 4096 steps/revolution (higher precision, lower torque)
//   • GEAR RATIO: 64:1 (internal gear reduction for high torque)
//   • STEP ANGLE: 5.625°/64 = 0.087890625° per step (very high precision)
//   • CURRENT CONFIGURATION: FULL4WIRE mode = 2048 steps/revolution
const int STEPS_PER_REVOLUTION = 2048;

// ============================================================================
// MOTOR CONFIGURATION VALUES - 28BYJ-48 STEPPER MOTOR SPECIFICATIONS
// ============================================================================

// Default motor speed: 1200 steps/second (increased for faster feeding)
// 28BYJ-48 SPEED LIMITS:
//   • MIN: 50 steps/sec (very slow, high torque, quiet operation)
//   • OPTIMAL: 200-800 steps/sec (good balance of speed/torque/precision)
//   • MAX RELIABLE: 1500 steps/sec (maximum speed with good reliability)
//   • ABSOLUTE MAX: 2000 steps/sec (may cause steps loss, overheating, noise)
//   • RECOMMENDED RANGE: 400-1200 steps/sec for feeding applications
const float DEFAULT_MAX_SPEED = 1200.0f;

// Default acceleration: 800 steps/second² (faster acceleration)
// 28BYJ-48 ACCELERATION LIMITS:
//   • MIN: 50 steps/sec² (very gentle start/stop, minimal vibration)
//   • OPTIMAL: 200-600 steps/sec² (smooth operation, good responsiveness)
//   • HIGH PERFORMANCE: 800-1200 steps/sec² (fast response, may cause vibration)
//   • MAX PRACTICAL: 1500 steps/sec² (rapid start/stop, increased wear)
//   • RECOMMENDED RANGE: 300-800 steps/sec² for reliable feeding operation
const float DEFAULT_ACCELERATION = 800.0f;

// Default motor rotation direction for feeding operations
// MOTOR DIRECTION CONFIGURATION:
//   • true = CLOCKWISE (CW): Motor rotates in positive direction
//   • false = COUNTER-CLOCKWISE (CCW): Motor rotates in negative direction
//   • RECOMMENDED: Test both directions to determine correct dispensing direction
//   • Can be changed at runtime via serial commands or web interface
//   • Stored in NVRAM for persistence across reboots
const bool DEFAULT_MOTOR_CLOCKWISE = false;

// NVRAM key for storing motor direction preference
const char* MOTOR_DIRECTION_NVRAM_KEY = "motor_direction";

// ============================================================================
// VIBRATION MOTOR CONFIGURATION VALUES
// ============================================================================

// Vibration motor pin assignment: GPIO 26 (supports PWM/LEDC)
// ESP32-WROOM-32 GPIO 26 SPECIFICATIONS:
//   • Supports LEDC PWM output (LED Control peripheral)
//   • Output-only pin, no input restrictions
//   • Not connected to internal flash, safe for any use
//   • Available on most ESP32 development boards
//   • 3.3V logic level output (drives NPN transistor base through 1kΩ resistor)
const uint8_t VIBRATION_MOTOR_PIN = 26;

// PWM channel for vibration motor (ESP32 has 16 LEDC channels: 0-15)
// Using channel 5 to avoid conflict with RGB LED (channels 0, 1, 2)
const uint8_t VIBRATION_PWM_CHANNEL = 5;

// PWM frequency: 1000Hz (1kHz) - good balance for motor control
// VIBRACALL MOTOR 1027 PWM FREQUENCY CONSIDERATIONS:
//   • 100Hz: Audible whine, rough vibration
//   • 500Hz: Slight audible tone, smooth vibration
//   • 1000Hz: Inaudible, very smooth vibration (RECOMMENDED)
//   • 5000Hz+: Silent, but may reduce motor torque
const uint32_t VIBRATION_PWM_FREQUENCY = 1000;

// PWM resolution: 8 bits (0-255 duty cycle range)
// 8-bit resolution provides 256 intensity levels (0-100% maps to 0-255)
const uint8_t VIBRATION_PWM_RESOLUTION = 8;

// Vibration motor maintenance task interval: 20ms
// This task monitors timed vibrations and handles automatic stop
const unsigned long VIBRATION_MAINTENANCE_INTERVAL = 20;

// ============================================================================
// RGB LED CONFIGURATION VALUES
// ============================================================================

// RGB LED pin assignments (ESP32-WROOM-32 GPIO pins with PWM support)
// GPIO 25, 27, 32 chosen for their PWM/LEDC capabilities and availability
// ESP32-WROOM-32 RGB LED PIN SPECIFICATIONS:
//   • GPIO 25: Output-only, supports LEDC PWM, safe for any use
//   • GPIO 27: Output-only, supports LEDC PWM, safe for any use  
//   • GPIO 32: ADC1_CH4, supports LEDC PWM, output capable
//   • All pins use 3.3V logic level
//   • Each channel requires 330Ω current-limiting resistor
const uint8_t RGB_LED_RED_PIN = 25;
const uint8_t RGB_LED_GREEN_PIN = 27;
const uint8_t RGB_LED_BLUE_PIN = 32;

// RGB LED type: 0 = common cathode (GND), 1 = common anode (VCC)
// COMMON CATHODE: LED on when pin HIGH (most common type)
// COMMON ANODE: LED on when pin LOW (inverts PWM signal)
// Hardware note: 4th pin connects to GND (cathode) or VCC (anode)
const uint8_t RGB_LED_TYPE = 0;  // 0 = COMMON_CATHODE

// RGB LED maintenance task interval: 20ms
// This task handles timed operations, fading, and blinking effects
// 20ms provides smooth visual updates (50Hz refresh rate)
const unsigned long RGB_LED_MAINTENANCE_INTERVAL = 20;

// ============================================================================
// TOUCH SENSOR CONFIGURATION VALUES (TTP223)
// ============================================================================

// Touch sensor pin assignment: GPIO 33 (input-only pin, suitable for sensors)
// ESP32-WROOM-32 GPIO 33 SPECIFICATIONS:
//   • Input-only pin (no output capability)
//   • Internal pull-up/pull-down available
//   • ADC1_CH5, RTC_GPIO8, TOUCH8
//   • Ideal for sensor inputs and touch detection
//   • Not connected to internal flash, safe for any use
const uint8_t TOUCH_SENSOR_PIN = 33;

// Touch sensor active logic: false = active HIGH (standard TTP223 behavior)
// TTP223 MODULE OUTPUT BEHAVIOR:
//   • Standard mode: OUTPUT = HIGH when touched, LOW when not touched
//   • Inverted mode: OUTPUT = LOW when touched, HIGH when not touched (rare)
//   • Most TTP223 modules use standard mode (active HIGH)
const bool TOUCH_SENSOR_ACTIVE_LOW = false;

// Touch sensor debounce delay: 50ms (prevents false triggers)
// DEBOUNCE DELAY CONSIDERATIONS:
//   • Too short (< 20ms): May cause false triggers from electrical noise
//   • Optimal (40-60ms): Good balance between responsiveness and stability
//   • Too long (> 100ms): Feels sluggish to user interaction
//   • Recommended: 50ms for responsive yet stable touch detection
//   • FAST RESPONSE: 20ms for minimal latency (if noise is not an issue)
const unsigned long TOUCH_SENSOR_DEBOUNCE_DELAY = 20;

// Touch sensor long press duration: 1000ms (1 second)
// LONG PRESS DURATION CONSIDERATIONS:
//   • Short (500-800ms): Quick access to long press features
//   • Standard (1000-1500ms): Clear distinction from normal touch
//   • Long (2000ms+): Prevents accidental long press activation
//   • Recommended: 1000ms for intuitive user experience
const unsigned long TOUCH_SENSOR_LONG_PRESS_DURATION = 1000;

// Touch sensor maintenance task interval: 5ms
// This task calls update() to handle debouncing and callbacks
// 5ms provides 200Hz update rate for highly responsive touch detection
// OPTIMIZATION: Reduced from 20ms to 5ms for faster tactile feedback
const unsigned long TOUCH_SENSOR_MAINTENANCE_INTERVAL = 5;

// Default number of portions to dispense on touch sensor long press
// TOUCH LONG PRESS FEEDING:
//   • User can hold touch sensor to trigger automatic feeding
//   • Configurable portion count (1-10 portions recommended)
//   • Stored in NVRAM for persistence across reboots
//   • Can be modified via serial commands or web interface
const uint8_t DEFAULT_TOUCH_LONG_PRESS_PORTIONS = 2;

// NVRAM key for storing touch long press portions preference
const char* TOUCH_LONG_PRESS_PORTIONS_NVRAM_KEY = "touch_portions";

// Touch sensor enabled/disabled state (default: enabled)
// When disabled, touch sensor won't trigger feeding or vibration
// LED status indications continue to work normally
const bool DEFAULT_TOUCH_SENSOR_ENABLED = true;

// NVRAM key for storing touch sensor enabled/disabled state
const char* TOUCH_SENSOR_ENABLED_NVRAM_KEY = "touch_enabled";

// Touch vibration feedback durations
// VIBRATION FEEDBACK DURATIONS:
//   • Short vibration: Quick tactile feedback on touch
//   • Long vibration: Extended feedback on long press detection
//   • Durations optimized for clear user perception
const unsigned long TOUCH_VIBRATION_SHORT_DURATION = 50;   // 50ms quick buzz
const unsigned long TOUCH_VIBRATION_LONG_DURATION = 200;   // 200ms extended buzz

// ============================================================================
// SERIAL COMMUNICATION CONFIGURATION VALUES
// ============================================================================

// Standard baud rate for ESP32 communication
const long SERIAL_BAUD_RATE = 115200;

// ============================================================================
// TASK SCHEDULER CONFIGURATION VALUES
// ============================================================================

// Display time every 1 second (1000ms)
const unsigned long DISPLAY_TIME_INTERVAL = 1000;

// Process serial commands every 50ms (responsive input)
const unsigned long SERIAL_PROCESS_INTERVAL = 50;

// Motor maintenance every 10ms (smooth stepper operation)
const unsigned long MOTOR_MAINTENANCE_INTERVAL = 10;

// Wait up to 3 seconds for serial connection
const unsigned long SERIAL_TIMEOUT = 3000;

// ============================================================================
// WIFI & BLUETOOTH CONFIGURATION VALUES
// ============================================================================

// WiFi connection timeout: 10 seconds
const unsigned long WIFI_CONNECTION_TIMEOUT = 10000;

// Auto-reconnection attempt every 30 seconds
const unsigned long WIFI_RECONNECT_INTERVAL = 30000;

// Maximum number of networks that can be saved
const int MAX_SAVED_NETWORKS = 10;

// Default Bluetooth device name
const char* DEFAULT_BLUETOOTH_NAME = "ESP32-FishFeeder";

// WiFi network scan timeout: 10 seconds
const unsigned long WIFI_SCAN_TIMEOUT = 10000;

// ============================================================================
// WIFI PORTAL CONFIGURATION VALUES
// ============================================================================

// Enable WiFi portal auto-start on system boot
const bool WIFI_PORTAL_AUTO_START = true;

// Enable WiFi portal when connection is lost
const bool WIFI_PORTAL_ON_DISCONNECT = true;

// WiFi portal timeout: 0 = never timeout (always active)
const unsigned long WIFI_PORTAL_TIMEOUT = 0;

// Default access point name for configuration portal
const char* WIFI_PORTAL_AP_NAME = "Fish Feeder";

// WiFi portal access password (empty string = no password required)
const char* WIFI_PORTAL_AP_PASSWORD = "0123456789";

// Check WiFi connection every 10 seconds
const unsigned long WIFI_CONNECTION_CHECK_INTERVAL = 10000;

// ============================================================================
// NTP TIME SYNCHRONIZATION VALUES
// ============================================================================

// NTP synchronization every 12 hours (43,200,000 milliseconds)
const unsigned long NTP_SYNC_INTERVAL = 12 * 60 * 60 * 1000;

// Intercalated time servers array (NTP and HTTP mixed)
// PRIORITY ORDER: Alternate between protocols for better reliability
// This ensures we try both NTP (UDP) and HTTP methods sequentially
const TimeServerEntry TIME_SERVERS[] = {
    {"ntp", "time.google.com"},                           // 1. Google NTP (very reliable)
    {"http", "worldtimeapi.org/api/timezone/America/Sao_Paulo"}, // 2. WorldTime HTTP API
    {"ntp", "time.cloudflare.com"},                       // 3. Cloudflare NTP (fast)
    {"http", "worldclockapi.com/api/json/utc/now"},       // 6. WorldClock HTTP API (UTC)
    {"ntp", "pool.ntp.org"},                              // 5. Global NTP pool
    {"ntp", "time.nist.gov"},                             // 7. NIST NTP (US official)
    {"ntp", "br.pool.ntp.org"},                           // 8. Brazil NTP pool
    {"ntp", "south-america.pool.ntp.org"},                // 9. South America NTP pool
    {"ntp", "0.pool.ntp.org"},                            // 10. Additional global pool
    {"ntp", "1.pool.ntp.org"}                             // 11. Additional global pool
};

// Number of time servers in the array
const int TIME_SERVERS_COUNT = sizeof(TIME_SERVERS) / sizeof(TIME_SERVERS[0]);

// DNS servers array (priority order: index 0 = highest priority)
const char* DNS_SERVERS[] = {
    "1.1.1.1",             // Secondary: Cloudflare DNS (fast and secure)
    "8.8.8.8",             // Primary: Google DNS (reliable worldwide)
    "1.0.0.1",             // Quaternary: Cloudflare DNS secondary
    "8.8.4.4",             // Tertiary: Google DNS secondary
    "208.67.222.222",      // Backup: OpenDNS primary
    "208.67.220.220"       // Backup: OpenDNS secondary
};

// Number of DNS servers in the array
const int DNS_SERVERS_COUNT = sizeof(DNS_SERVERS) / sizeof(DNS_SERVERS[0]);

// GMT offset for Brazil Standard Time (UTC-3 = -3 * 3600 seconds)
const long GMT_OFFSET_SEC = -3 * 3600;

// Daylight saving time offset (0 = no DST, Brazil varies by year)
const int DAYLIGHT_OFFSET_SEC = 0;

// NTP synchronization timeout: 10 seconds (faster response, multiple servers available)
const unsigned long NTP_SYNC_TIMEOUT = 10000;

// HTTP time fallback timeout: 8 seconds (quick HTTP response expected)
const unsigned long HTTP_TIME_TIMEOUT = 8000;

// Wait 5 seconds after WiFi connection before first NTP sync
const unsigned long NTP_INITIAL_SYNC_DELAY = 5000;

// NVRAM key for storing last NTP sync timestamp
const char* NTP_LAST_SYNC_NVRAM_KEY = "ntp_last_sync";

// ============================================================================
// FEEDING SCHEDULE CONFIGURATION VALUES
// ============================================================================

// Schedule monitoring every 30 seconds (30,000 milliseconds)
const unsigned long FEEDING_SCHEDULE_MONITOR_INTERVAL = 30000;

// Allow up to 30 minutes tolerance for missed feedings
const uint16_t FEEDING_SCHEDULE_TOLERANCE_MINUTES = 30;

// Look back up to 12 hours for missed feedings after power loss
const uint16_t FEEDING_SCHEDULE_MAX_RECOVERY_HOURS = 12;

// Maximum number of scheduled feedings that can be configured
const uint8_t MAX_SCHEDULED_FEEDINGS = 10;

/**
 * Default Feeding Schedule Configuration
 * 
 * This schedule provides typical feeding times for aquarium fish:
 * - Morning feeding: 08:00 (2 portions)
 * - Afternoon feeding: 12:00 (1 portion) 
 * - Evening feeding: 18:00 (2 portions)
 * 
 * Users can modify these times and portions as needed for their specific fish.
 */
ScheduledFeeding DEFAULT_FEEDING_SCHEDULE[] = {
    {8,  0, 0, 2, true, "Morning feeding"},      // 08:00:00 - 2 portions
    {12, 0, 0, 1, true, "Midday feeding"},       // 12:00:00 - 1 portion
    {18, 0, 0, 2, true, "Evening feeding"}       // 18:00:00 - 2 portions
};

// Number of default scheduled feedings
const uint8_t DEFAULT_SCHEDULE_COUNT = sizeof(DEFAULT_FEEDING_SCHEDULE) / sizeof(DEFAULT_FEEDING_SCHEDULE[0]);

// ============================================================================
// HELPER FUNCTION IMPLEMENTATIONS
// ============================================================================

/**
 * Convert portions to motor steps
 * 
 * @param portions: Number of food portions
 * @return: Number of steps needed
 */
int portionsToSteps(int portions) {
    // Clamp to valid range first
    portions = clampPortions(portions);
    
    // Calculate steps: portions * (rotation per portion) * (steps per revolution)
    return (int)(portions * FOOD_PORTION_ROTATION * STEPS_PER_REVOLUTION);
}

/**
 * Validate if portion count is within allowed range
 * 
 * @param portions: Number of portions to validate
 * @return: true if valid, false otherwise
 */
bool isValidPortionCount(int portions) {
    return (portions >= MIN_FOOD_PORTIONS && portions <= MAX_FOOD_PORTIONS);
}

/**
 * Clamp portion count to valid range
 * 
 * @param portions: Requested portions
 * @return: Clamped value within MIN_FOOD_PORTIONS to MAX_FOOD_PORTIONS
 */
int clampPortions(int portions) {
    if (portions < MIN_FOOD_PORTIONS) {
        return MIN_FOOD_PORTIONS;
    }
    if (portions > MAX_FOOD_PORTIONS) {
        return MAX_FOOD_PORTIONS;
    }
    return portions;
}