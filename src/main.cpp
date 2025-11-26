#include <Arduino.h>
#include <EEPROM.h>
#include <TaskScheduler.h>
#include <Preferences.h>
#include "module_manager.h"
#include "rtc_module.h"
#include "stepper_motor.h"
#include "feeding_controller.h"
#include "feeding_schedule.h"
#include "wifi_controller.h"
#include "ntp_sync.h"
#include "vibration_motor.h"
#include "rgb_led.h"
#include "touch_sensor.h"
#include "config.h"
#include "console_manager.h"
#include "command_listener.h"

// ============================================================================
// GLOBAL CONFIGURATION VARIABLES
// ============================================================================

// Touch long press portions - configurable via NVRAM
uint8_t touchLongPressPortions = DEFAULT_TOUCH_LONG_PRESS_PORTIONS;

// Touch sensor enabled/disabled state - configurable via NVRAM
bool touchSensorEnabled = DEFAULT_TOUCH_SENSOR_ENABLED;

// LED Status Control - NEW ARCHITECTURE
// Centralized LED status management based on system state priority
enum SystemLEDState {
    LED_STATE_READY,          // Green solid - System ready
    LED_STATE_FEEDING,        // Green blinking - Feeding in progress
    LED_STATE_CANCEL_FLASH,   // Red flash - Feeding canceled
    LED_STATE_ERROR           // Red solid - Error state
};

SystemLEDState currentLEDState = LED_STATE_READY;
SystemLEDState desiredLEDState = LED_STATE_READY;
unsigned long ledStateChangeTime = 0;
const unsigned long CANCEL_FLASH_DURATION = 300;  // 300ms red flash

// Preferences for NVRAM storage
Preferences touchPreferences;

// ============================================================================
// MODULE MANAGER - CENTRALIZED MODULE REFERENCE MANAGEMENT
// ============================================================================

// Create ModuleManager instance for centralized module access
ModuleManager moduleManager;

// ============================================================================
// HARDWARE MODULE INSTANCES
// ============================================================================

// Create RTC module instance
RTCModule rtcModule;

// Create stepper motor instance
// Pinos finais para ESP32 DevKit V1 30-pin CH9102X (sem conflito com RTC I2C)
StepperMotor feedMotor(15, 4, 5, 18);

// Create vibration motor instance
// GPIO 26 with PWM channel 1, 1kHz frequency, 8-bit resolution
VibrationMotor vibrationMotor(VIBRATION_MOTOR_PIN, VIBRATION_PWM_CHANNEL, 
                              VIBRATION_PWM_FREQUENCY, VIBRATION_PWM_RESOLUTION);

// Create RGB LED instance
// GPIO 25 (Red), 27 (Green), 32 (Blue) - Common Cathode
RGBLed rgbLed(RGB_LED_RED_PIN, RGB_LED_GREEN_PIN, RGB_LED_BLUE_PIN, 
              RGB_LED_TYPE == 0 ? RGBLed::COMMON_CATHODE : RGBLed::COMMON_ANODE);

// Create touch sensor instance
// GPIO 33 - Input only pin, ideal for sensors
TouchSensor touchSensor(TOUCH_SENSOR_PIN, TOUCH_SENSOR_ACTIVE_LOW);

// ============================================================================
// CONTROLLER MODULE INSTANCES
// ============================================================================

// Create feeding controller
FeedingController feedingController(&feedMotor);

// Create feeding schedule system
FeedingSchedule feedingSchedule;

// Create WiFi controller instance
WiFiController wifiController;

// Create NTP synchronization module
NTPSync ntpSync(&moduleManager);

// Create command listener with ModuleManager
CommandListener commandListener(&moduleManager);

// ============================================================================
// TASK SCHEDULER SETUP
// ============================================================================

// Create scheduler instance
Scheduler taskScheduler;

// ============================================================================
// FORWARD DECLARATIONS - CENTRALIZED FEEDING FUNCTIONS
// ============================================================================

// These functions are used by multiple modules and must be declared before usage
bool startFeeding(uint8_t portions, bool recordInSchedule = true);
bool cancelFeeding();
uint8_t getTouchLongPressPortions();
void setTouchLongPressPortions(uint8_t portions);
bool getTouchSensorEnabled();
void setTouchSensorEnabled(bool enabled);

// LED status management functions
void updateLEDStatus();
void applyLEDState(SystemLEDState state);

// ============================================================================
// TASK CALLBACK FORWARD DECLARATIONS
// ============================================================================

// Task callback functions
void displayTimeTask();
void processSerialTask();
void motorMaintenanceTask();
void vibrationMaintenanceTask();
void rgbLedMaintenanceTask();
void touchSensorMaintenanceTask();
void feedingMonitorTask();
void scheduleMonitorTask();
void wifiMonitorTask();
void ntpSyncTask();
void wifiPortalTask();

// Task definitions using configuration constants
Task tDisplayTime(DISPLAY_TIME_INTERVAL, TASK_FOREVER, &displayTimeTask, &taskScheduler, true);
Task tProcessSerial(SERIAL_PROCESS_INTERVAL, TASK_FOREVER, &processSerialTask, &taskScheduler, true);
Task tMotorMaintenance(MOTOR_MAINTENANCE_INTERVAL, TASK_FOREVER, &motorMaintenanceTask, &taskScheduler, true);
Task tVibrationMaintenance(VIBRATION_MAINTENANCE_INTERVAL, TASK_FOREVER, &vibrationMaintenanceTask, &taskScheduler, true);
Task tRGBLedMaintenance(RGB_LED_MAINTENANCE_INTERVAL, TASK_FOREVER, &rgbLedMaintenanceTask, &taskScheduler, true);
Task tTouchSensorMaintenance(TOUCH_SENSOR_MAINTENANCE_INTERVAL, TASK_FOREVER, &touchSensorMaintenanceTask, &taskScheduler, true);
Task tFeedingMonitor(100, TASK_FOREVER, &feedingMonitorTask, &taskScheduler, false); // Start disabled
Task tScheduleMonitor(FEEDING_SCHEDULE_MONITOR_INTERVAL, TASK_FOREVER, &scheduleMonitorTask, &taskScheduler, true);
Task tWiFiMonitor(WIFI_CONNECTION_CHECK_INTERVAL, TASK_FOREVER, &wifiMonitorTask, &taskScheduler, true);
Task tNTPSync(60000, TASK_FOREVER, &ntpSyncTask, &taskScheduler, true); // Check every minute
Task tWiFiPortal(500, TASK_FOREVER, &wifiPortalTask, &taskScheduler, true); // Process portal every 500ms

// ============================================================================
// TASK CALLBACK IMPLEMENTATIONS
// ============================================================================

/**
 * Task: Display current date and time
 * Runs every 1 second but only shows time when explicitly requested
 */
void displayTimeTask() {
    // Time display is now controlled by explicit TIME command
    // This task remains active for future use but doesn't print automatically
}

/**
 * Task: Process serial commands
 * Runs every 50ms to check for incoming serial commands
 */
void processSerialTask() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        commandListener.processCommand(command);
    }
}

/**
 * Task: Motor maintenance and non-blocking operations
 * Runs every 10ms to handle motor operations and stepper updates
 */
void motorMaintenanceTask() {
    // Run stepper motor for non-blocking operations
    feedMotor.run();
}

/**
 * Task: Vibration motor maintenance
 * Runs every 20ms to handle timed vibration auto-stop
 */
void vibrationMaintenanceTask() {
    vibrationMotor.updateState();
}

/**
 * Update LED based on system state
 * This is the SINGLE SOURCE OF TRUTH for LED status
 * Called by rgbLedMaintenanceTask() every 20ms
 */
void updateLEDStatus() {
    // Determine desired state based on system status (priority order)
    if (moduleManager.getFeedingInProgress()) {
        desiredLEDState = LED_STATE_FEEDING;
    } else {
        desiredLEDState = LED_STATE_READY;
    }
    
    // Handle temporary cancel flash
    if (currentLEDState == LED_STATE_CANCEL_FLASH) {
        if (millis() - ledStateChangeTime >= CANCEL_FLASH_DURATION) {
            // Cancel flash expired - transition to desired state
            currentLEDState = desiredLEDState;
            applyLEDState(currentLEDState);
        }
        // Still flashing, don't change
        return;
    }
    
    // Apply desired state if different from current
    if (desiredLEDState != currentLEDState) {
        currentLEDState = desiredLEDState;
        applyLEDState(currentLEDState);
    }
}

/**
 * Apply LED state to hardware
 */
void applyLEDState(SystemLEDState state) {
    switch (state) {
        case LED_STATE_READY:
            rgbLed.setDeviceStatus(RGBLed::STATUS_READY);
            Console::println(F("LED: READY (green solid)"));
            break;
            
        case LED_STATE_FEEDING:
            rgbLed.setDeviceStatus(RGBLed::STATUS_FEEDING);
            Console::println(F("LED: FEEDING (green blink)"));
            break;
            
        case LED_STATE_CANCEL_FLASH:
            rgbLed.stopBlink();
            rgbLed.setColor(255, 0, 0);
            rgbLed.turnOn();
            ledStateChangeTime = millis();
            Console::println(F("LED: CANCEL FLASH (red)"));
            break;
            
        case LED_STATE_ERROR:
            rgbLed.setColor(255, 0, 0);
            rgbLed.turnOn();
            Console::println(F("LED: ERROR (red solid)"));
            break;
    }
}

/**
 * Task: RGB LED maintenance
 * Runs every 20ms to handle LED updates and state management
 */
void rgbLedMaintenanceTask() {
    // Update LED hardware (blinking, fading, etc)
    rgbLed.update();
    
    // Update LED status based on system state
    updateLEDStatus();
}

/**
 * Task: Touch sensor maintenance
 * Runs every 20ms to handle touch detection, debouncing, and callbacks
 */
void touchSensorMaintenanceTask() {
    touchSensor.update();
}

/**
 * Task: Monitor feeding operations
 * Runs every 100ms to check if async feeding is complete
 */
void feedingMonitorTask() {
    static bool wasFeeding = false;
    
    if (moduleManager.getFeedingInProgress() && !feedMotor.isRunning()) {
        // Feeding completed
        Console::printlnR(F("Food dispensing completed successfully"));
        moduleManager.setFeedingInProgress(false);
        tFeedingMonitor.disable();
        wasFeeding = false;
        // LED will automatically transition to READY via updateLEDStatus()
    } else if (moduleManager.getFeedingInProgress() && !wasFeeding) {
        // Feeding just started
        Console::println(F("Feeding in progress detected"));
        wasFeeding = true;
        // LED will automatically show FEEDING via updateLEDStatus()
    }
}

/**
 * Task: Monitor feeding schedule for automatic feeding
 * Runs every 30 seconds to check for scheduled feeding times
 */
void scheduleMonitorTask() {
    // Get current time from RTC
    DateTime currentTime = rtcModule.now();
    
    // Process schedules - this handles all scheduled feeding logic
    feedingSchedule.processSchedules(currentTime);
    
    // If scheduled feeding triggered manual feeding, update the schedule system
    // (This logic is handled by the existing feedingMonitorTask)
}

/**
 * Task: Monitor WiFi connection status
 * Runs every 10 seconds to check connection and handle auto-reconnection
 */
void wifiMonitorTask() {
    // 🚨 CRITICAL: Continuously verify LED status matches actual WiFi state
    // This ensures LED always reflects the true connection state
    
    static bool wasConnected = false;
    bool isConnected = wifiController.isWiFiConnected();
    
    // Detect connection state changes
    if (!isConnected && wasConnected) {
        // Just lost connection
        Console::printlnR(F("WiFi connection lost!"));
    } else if (isConnected && !wasConnected) {
        // Just got connected
        Console::printlnR(F("WiFi connection established - notifying NTP module"));
        ntpSync.onWiFiConnected();
    }
    
    // 🚨 CRITICAL: Ensure LED status is always correct
    // Check every cycle to prevent LED showing wrong state
    if (isConnected) {
        // Connected: LED should be GREEN (unless feeding is in progress)
        if (rgbLed.getDeviceStatus() == RGBLed::STATUS_WIFI_CONNECTING || 
            rgbLed.getDeviceStatus() == RGBLed::STATUS_WIFI_ERROR) {
            // Was showing WiFi status, now we're connected
            if (!moduleManager.getFeedingInProgress()) {
                rgbLed.setDeviceStatus(RGBLed::STATUS_READY);
            }
        }
    } else {
        // Not connected: LED should show trying or error
        if (rgbLed.getDeviceStatus() == RGBLed::STATUS_READY) {
            // Was ready, but connection lost - show error
            rgbLed.setDeviceStatus(RGBLed::STATUS_WIFI_ERROR);
        }
    }
    
    wifiController.checkConnectionStatus();
    wifiController.handleAutoReconnect();
    
    wasConnected = isConnected;
}

/**
 * Task: Handle NTP time synchronization
 * Runs every minute to check if NTP sync is needed
 */
void ntpSyncTask() {
    // 🚨 STATUS: TIME_SYNCING - Yellow 50% blinking 500ms
    static bool wasSyncing = false;
    bool isSyncing = ntpSync.isSyncInProgress();
    
    // Detect sync start
    if (isSyncing && !wasSyncing) {
        // Only change status if we're in ready state (don't override feeding)
        if (rgbLed.getDeviceStatus() == RGBLed::STATUS_READY) {
            rgbLed.setDeviceStatus(RGBLed::STATUS_TIME_SYNCING);
        }
    } else if (!isSyncing && wasSyncing) {
        // Sync completed - return to ready if we were syncing
        if (rgbLed.getDeviceStatus() == RGBLed::STATUS_TIME_SYNCING) {
            rgbLed.setDeviceStatus(RGBLed::STATUS_READY);
        }
    }
    
    ntpSync.handleNTPSync();
    wasSyncing = isSyncing;
}

// ============================================================================
// TASK CONTROL FUNCTIONS FOR CONSOLE MANAGER
// ============================================================================

void pauseDisplayTask() {
    tDisplayTime.disable();
}

void resumeDisplayTask() {
    tDisplayTime.enable();
}

void pauseMotorTask() {
    tMotorMaintenance.disable();
}

void resumeMotorTask() {
    tMotorMaintenance.enable();
}

void enableFeedingMonitor() {
    tFeedingMonitor.enable();
}

void showTaskStatus() {
    Console::printlnR(F("=== TASK SCHEDULER STATUS ==="));
    Console::printR(F("Total Tasks: "));
    Console::printlnR(String(taskScheduler.getTotalTasks()));
    Console::printR(F("Active Tasks: "));
    Console::printlnR(String(taskScheduler.getActiveTasks()));
    Console::printR(F("Invoked Tasks (last cycle): "));
    Console::printlnR(String(taskScheduler.getInvokedTasks()));
    
    Console::printlnR(F(""));
    Console::printlnR(F("Task Details:"));
    Console::printR(F("Display Time Task - Enabled: "));
    Console::printR(tDisplayTime.isEnabled() ? F("Yes") : F("No"));
    Console::printR(F(", Interval: "));
    Console::printR(String(tDisplayTime.getInterval()));
    Console::printlnR(F("ms"));
    
    Console::printR(F("Process Serial Task - Enabled: "));
    Console::printR(tProcessSerial.isEnabled() ? F("Yes") : F("No"));
    Console::printR(F(", Interval: "));
    Console::printR(String(tProcessSerial.getInterval()));
    Console::printlnR(F("ms"));
    
    Console::printR(F("Motor Maintenance Task - Enabled: "));
    Console::printR(tMotorMaintenance.isEnabled() ? F("Yes") : F("No"));
    Console::printR(F(", Interval: "));
    Console::printR(String(tMotorMaintenance.getInterval()));
    Console::printlnR(F("ms"));
    
    Console::printR(F("Feeding Monitor Task - Enabled: "));
    Console::printR(tFeedingMonitor.isEnabled() ? F("Yes") : F("No"));
    Console::printR(F(", Interval: "));
    Console::printR(String(tFeedingMonitor.getInterval()));
    Console::printlnR(F("ms"));
    
    Console::printR(F("WiFi Monitor Task - Enabled: "));
    Console::printR(tWiFiMonitor.isEnabled() ? F("Yes") : F("No"));
    Console::printR(F(", Interval: "));
    Console::printR(String(tWiFiMonitor.getInterval()));
    Console::printlnR(F("ms"));
    
    Console::printR(F("NTP Sync Task - Enabled: "));
    Console::printR(tNTPSync.isEnabled() ? F("Yes") : F("No"));
    Console::printR(F(", Interval: "));
    Console::printR(String(tNTPSync.getInterval()));
    Console::printlnR(F("ms"));
    
    Console::printlnR(F(""));
    Console::printR(F("Feeding in Progress: "));
    Console::printlnR(moduleManager.getFeedingInProgress() ? F("Yes") : F("No"));
    
    Console::printR(F("Logging Enabled: "));
    Console::printlnR(ConsoleManager::isLoggingEnabled ? F("Yes") : F("No"));
    
    Console::printlnR(F("============================"));
}

// ============================================================================
// TOUCH SENSOR CALLBACK
// ============================================================================

/**
 * Touch sensor event callback
 * 
 * NEW BEHAVIOR:
 * - TOUCH_PRESSED: Quick short vibration (50ms) for tactile feedback
 * - TOUCH_RELEASED: Stop vibration
 * - TOUCH_LONG_PRESS:
 *   • If feeding in progress: Cancel feeding + Red LED for 1s + Longer vibration (200ms)
 *   • If not feeding: Start feeding with configured portions + Longer vibration (200ms)
 */
void onTouchEvent(TouchSensor::TouchEvent event, unsigned long duration) {
    switch (event) {
        case TouchSensor::TOUCH_PRESSED:
            // Quick short vibration on touch (only if touch sensor is enabled)
            if (touchSensorEnabled) {
                vibrationMotor.startTimed(60, TOUCH_VIBRATION_SHORT_DURATION);  // 60% for 50ms
            }
            Console::println(F("Touch pressed"));
            break;
            
        case TouchSensor::TOUCH_RELEASED:
            // Touch released - no action needed (vibration auto-stops with timed)
            Console::print(F("Touch released ("));
            Console::print(String(duration));
            Console::println(F("ms)"));
            break;
            
        case TouchSensor::TOUCH_LONG_PRESS:
            Console::print(F("Long press detected ("));
            Console::print(String(duration));
            Console::println(F("ms)"));
            
            // Check if touch sensor is enabled
            if (!touchSensorEnabled) {
                Console::println(F("Touch sensor disabled - ignoring long press"));
                return;
            }
            
            // Longer vibration for long press feedback (reduced intensity)
            vibrationMotor.startTimed(60, TOUCH_VIBRATION_LONG_DURATION);  // 60% for 200ms
            
            // Check if feeding is currently in progress
            if (moduleManager.getFeedingInProgress()) {
                // CANCEL FEEDING - Use centralized method
                cancelFeeding();
            } else {
                // START FEEDING - Use centralized method with configured portions
                startFeeding(touchLongPressPortions, true);
            }
            break;
    }
}

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  
  // Wait for serial connection for debugging (non-blocking)
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < SERIAL_TIMEOUT)) {
    // Wait for serial connection with configurable timeout
  }
  
  Console::printlnR(F("=== Fish Feeder System Starting ==="));
  Console::printlnR(F("ESP32 - TaskScheduler-based Non-blocking Architecture"));
  Console::printlnR(F("Using ModuleManager for centralized module management"));
  
  // ========================================================================
  // CRITICAL: REGISTER ALL MODULES WITH MODULE MANAGER FIRST
  // ========================================================================
  Console::printlnR(F(""));
  Console::printlnR(F("=== Registering Modules with ModuleManager ==="));
  
  moduleManager.registerRTCModule(&rtcModule);
  moduleManager.registerStepperMotor(&feedMotor);
  moduleManager.registerFeedingController(&feedingController);
  moduleManager.registerFeedingSchedule(&feedingSchedule);
  moduleManager.registerWiFiController(&wifiController);
  moduleManager.registerNTPSync(&ntpSync);
  moduleManager.registerVibrationMotor(&vibrationMotor);
  moduleManager.registerRGBLed(&rgbLed);
  moduleManager.registerTouchSensor(&touchSensor);
  
  Console::printlnR(F("✓ All modules registered with ModuleManager"));
  Console::printlnR(F("==========================================="));
  Console::printlnR(F(""));
  
  // 🚨 CRITICAL: Initialize RGB LED FIRST for status indication
  if (rgbLed.begin()) {
    Console::printlnR(F("RGB LED: Initialized - Setting BOOTING status"));
    // STATUS: BOOTING - Red 50% blinking 500ms
    rgbLed.setDeviceStatus(RGBLed::STATUS_BOOTING);
  } else {
    Console::printlnR(F("ERROR: Failed to initialize RGB LED"));
  }
  
  // Initialize touch sensor
  if (touchSensor.begin(false)) {  // false = no internal pull-up
    Console::printR(F("Touch sensor: Initialized on pin "));
    Console::println(String(TOUCH_SENSOR_PIN));
    touchSensor.setDebounceDelay(TOUCH_SENSOR_DEBOUNCE_DELAY);
    touchSensor.setLongPressDuration(TOUCH_SENSOR_LONG_PRESS_DURATION);
    
    // Register callback for touch events (vibration feedback)
    touchSensor.setCallback(onTouchEvent);
    Console::printlnR(F("Touch sensor callback registered (vibration feedback)"));
    
    // Load touch long press portions from NVRAM
    touchPreferences.begin("touch", false);
    touchLongPressPortions = touchPreferences.getUChar(TOUCH_LONG_PRESS_PORTIONS_NVRAM_KEY, DEFAULT_TOUCH_LONG_PRESS_PORTIONS);
    touchSensorEnabled = touchPreferences.getBool(TOUCH_SENSOR_ENABLED_NVRAM_KEY, DEFAULT_TOUCH_SENSOR_ENABLED);
    touchPreferences.end();
    Console::printR(F("Touch long press portions loaded from NVRAM: "));
    Console::printlnR(String(touchLongPressPortions));
    Console::printR(F("Touch sensor enabled: "));
    Console::printlnR(touchSensorEnabled ? F("YES") : F("NO"));
  } else {
    Console::printlnR(F("ERROR: Failed to initialize touch sensor"));
  }
  
  // Initialize RTC module
  if (!rtcModule.begin()) {
    Console::printlnR(F("RTC initialization failed. System will continue with limited functionality."));
    Console::printlnR(F("Run 'rtcModule.scanI2C()' for manual diagnostics."));
  }
  
  // Keep LED blinking during boot (non-blocking updates)
  for (int i = 0; i < 10; i++) {
    rgbLed.update();
    delay(50);  // Small delays distributed throughout boot
  }
  
  // Initialize stepper motor
  if (!feedMotor.begin()) {
    Console::printlnR(F("ERROR: Failed to initialize stepper motor"));
    Console::printlnR(F("Check connections (ESP32 DevKit V1 30-pin):"));
    Console::printlnR(F("ULN2003 Motor Driver:"));
    Console::printlnR(F("- IN1 -> GPIO 15"));
    Console::printlnR(F("- IN2 -> GPIO 4"));
    Console::printlnR(F("- IN3 -> GPIO 5"));
    Console::printlnR(F("- IN4 -> GPIO 18"));
    Console::printlnR(F("- VCC -> 5V Direct"));
    Console::printlnR(F("- GND -> GND"));
    Console::printlnR(F("RTC DS3231 (when connected):"));
    Console::printlnR(F("- SDA -> GPIO 21"));
    Console::printlnR(F("- SCL -> GPIO 22"));
  } else {
    feedMotor.setMaxSpeed(DEFAULT_MAX_SPEED);
    feedMotor.setAcceleration(DEFAULT_ACCELERATION);
    
    // Initialize feeding controller
    if (!feedingController.begin()) {
      Console::printlnR(F("ERROR: Failed to initialize feeding controller"));
    }
  }
  
  // Keep LED blinking during boot
  for (int i = 0; i < 10; i++) {
    rgbLed.update();
    delay(50);
  }
  
  // Initialize vibration motor
  if (!vibrationMotor.begin()) {
    Console::printlnR(F("ERROR: Failed to initialize vibration motor"));
    Console::printlnR(F("Check connections (ESP32 DevKit V1 30-pin):"));
    Console::printlnR(F("Vibracall Motor 1027 (3V) via NPN 2N2222:"));
    Console::printlnR(F("- GPIO 26 -> 1kΩ -> Base NPN 2N2222"));
    Console::printlnR(F("- Collector -> Motor -> 3V"));
    Console::printlnR(F("- Emitter -> GND"));
    Console::printlnR(F("- 1N4007 diode across motor (cathode to 3V)"));
    Console::printlnR(F("- 100nF capacitor across motor"));
  } else {
    Console::printlnR(F("Vibration Motor: Initialized on GPIO 26"));
    // Ensure motor is completely off after initialization
    vibrationMotor.stop();
    Console::printlnR(F("Vibration Motor: Confirmed OFF state"));
  }
  
  // Keep LED blinking during boot
  for (int i = 0; i < 10; i++) {
    rgbLed.update();
    delay(50);
  }
  
  Console::printlnR(F("=== Transitioning to WiFi Connection Phase ==="));
  
  // 🚨 STATUS: WIFI_CONNECTING - Blue 50% blinking 500ms
  rgbLed.setDeviceStatus(RGBLed::STATUS_WIFI_CONNECTING);
  
  // Keep LED blinking during WiFi init
  for (int i = 0; i < 10; i++) {
    rgbLed.update();
    delay(50);
  }
  
  // Initialize WiFi Controller
  if (!wifiController.begin()) {
    Console::printlnR(F("WARNING: Failed to initialize WiFi Controller"));
    Console::printlnR(F("WiFi functions will be limited"));
  }
  
  // Configure WiFi Controller with RGB LED reference for status indication
  wifiController.setRGBLed(&rgbLed);
  Console::printlnR(F("WiFi Controller: RGB LED integration configured"));
  
  // Keep LED blinking during WiFi connection (longer period)
  Console::printlnR(F("Waiting for WiFi connection..."));
  unsigned long wifiStartTime = millis();
  while (!wifiController.isWiFiConnected() && (millis() - wifiStartTime < 10000)) {
    rgbLed.update();
    delay(100);  // Update LED while waiting
  }
  
  // Configure WiFi Controller with ModuleManager reference for web interface
  wifiController.setModuleManager(&moduleManager);
  
  // Keep LED blinking
  for (int i = 0; i < 10; i++) {
    rgbLed.update();
    delay(50);
  }
  
  // Initialize NTP Synchronization
  if (!ntpSync.begin()) {
    Console::printlnR(F("WARNING: Failed to initialize NTP synchronization"));
    Console::printlnR(F("Automatic time sync will not be available"));
  }
  
  // Keep LED blinking
  for (int i = 0; i < 10; i++) {
    rgbLed.update();
    delay(50);
  }
  
  // Initialize Feeding Schedule System
  feedingSchedule.begin(&moduleManager);
  // Note: Schedules are now loaded automatically from NVRAM in begin()
  // DEFAULT_FEEDING_SCHEDULE is only used on first boot or NVRAM reset
  Console::printlnR(F("Feeding Schedule: System initialized with persistent schedules"));
  
  // 🚨 CRITICAL: Register feeding monitor callback so schedule can enable LED monitoring
  feedingSchedule.setEnableMonitorCallback(enableFeedingMonitor);
  Console::printlnR(F("Feeding Schedule: Monitor callback registered"));
  
  // Configure WiFi Controller with ModuleManager reference for web interface
  wifiController.setModuleManager(&moduleManager);
  
  // CRITICAL: Register ALL endpoints now that components are ready
  Console::printlnR("=== FINAL ENDPOINT REGISTRATION ===");
  wifiController.registerAllEndpoints();
  Console::printlnR("=== ALL ENDPOINTS REGISTERED ===");
  
  // Show current date and time
  DateTime now = rtcModule.now();
  Console::printR(F("Current Date/Time: "));
  rtcModule.printDateTime();
  
  // Initialize and start task scheduler
  Console::printlnR(F("\nStarting Task Scheduler..."));
  Console::printR(F("Tasks configured: "));
  Console::printlnR(String(taskScheduler.getTotalTasks()));
  
  // Display task information with actual intervals
  Console::printR(F("- Display Time: Every "));
  Console::printR(String(DISPLAY_TIME_INTERVAL));
  Console::printlnR(F("ms"));
  Console::printR(F("- Process Serial: Every "));
  Console::printR(String(SERIAL_PROCESS_INTERVAL));
  Console::printlnR(F("ms"));
    Console::printR(F("- Motor Maintenance: Every "));
    Console::printR(String(MOTOR_MAINTENANCE_INTERVAL));
    Console::printlnR(F("ms"));
  Console::printR(F("- Schedule Monitor: Every "));
  Console::printR(String(FEEDING_SCHEDULE_MONITOR_INTERVAL));
  Console::printlnR(F("ms"));
  Console::printR(F("- WiFi Monitor: Every "));
  Console::printR(String(WIFI_CONNECTION_CHECK_INTERVAL));
  Console::printlnR(F("ms"));
  Console::printR(F("- NTP Sync: Every "));
  Console::printR(String(60000));
  Console::printlnR(F("ms (check interval)"));
  Console::printR(F("- WiFi Portal: Every "));
  Console::printR(String(500));
  Console::printlnR(F("ms (non-blocking)"));
  Console::printlnR(F("System ready - Non-blocking operation active"));
  
  // 🚨 STATUS: READY - Green 60% static
  rgbLed.setDeviceStatus(RGBLed::STATUS_READY);
}

// ============================================================================
// HELPER FUNCTIONS FOR EXTERNAL ACCESS
// ============================================================================

/**
 * Start feeding operation (centralized method for all sources)
 * 
 * This method ensures consistent behavior across all feeding sources:
 * - Manual feeding via serial command
 * - Scheduled automatic feeding
 * - Touch sensor long press
 * - WiFi web interface
 * - API endpoints
 * 
 * @param portions: Number of portions to dispense
 * @param recordInSchedule: Whether to record this as manual feeding in schedule
 * @return: true if feeding started successfully, false otherwise
 */
bool startFeeding(uint8_t portions, bool recordInSchedule) {
    // Validate portions
    if (portions < MIN_FOOD_PORTIONS || portions > MAX_FOOD_PORTIONS) {
        String msg = String(F("✗ Invalid portion count: ")) + String(portions);
        Console::printlnR(msg);
        return false;
    }
    
    // Check if already feeding
    if (moduleManager.getFeedingInProgress()) {
        Console::printlnR(F("✗ Feeding already in progress"));
        return false;
    }
    
    // Check if controller is ready
    if (!feedingController.isReady()) {
        Console::printlnR(F("✗ Feeding controller not ready"));
        return false;
    }
    
    String msg = String(F("▶ Starting feeding: ")) + String(portions) + String(F(" portions"));
    Console::printlnR(msg);
    
    // Start async feeding
    if (feedingController.dispenseFoodAsync(portions)) {
        // Mark feeding as in progress
        moduleManager.setFeedingInProgress(true);
        
        // Enable monitoring task
        tFeedingMonitor.enable();
        
        // LED will automatically transition to FEEDING via updateLEDStatus()
        
        // Record in schedule system if requested
        if (recordInSchedule && moduleManager.hasFeedingSchedule() && moduleManager.hasRTCModule()) {
            DateTime now = moduleManager.getRTCModule()->now();
            moduleManager.getFeedingSchedule()->recordManualFeeding(now);
        }
        
        Console::printlnR(F("✓ Feeding started successfully"));
        return true;
    }
    
    Console::printlnR(F("✗ Failed to start feeding"));
    return false;
}

/**
 * Cancel ongoing feeding operation
 * Can be called from any source (touch sensor, API, command)
 * 
 * @return: true if feeding was canceled, false if no feeding in progress
 */
bool cancelFeeding() {
    if (!moduleManager.getFeedingInProgress()) {
        Console::printlnR(F("ℹ No feeding in progress to cancel"));
        return false;
    }
    
    Console::printlnR(F("⚠ Canceling feeding operation..."));
    
    // Stop motor immediately - clears target position
    feedMotor.stop();
    
    // Mark feeding as completed
    moduleManager.setFeedingInProgress(false);
    
    // Disable monitoring task
    tFeedingMonitor.disable();
    
    // Trigger cancel flash - will auto-transition to READY after timeout
    currentLEDState = LED_STATE_CANCEL_FLASH;
    applyLEDState(LED_STATE_CANCEL_FLASH);
    
    Console::printlnR(F("✓ Feeding canceled successfully"));
    return true;
}

/**
 * Get current touch long press portions setting
 */
uint8_t getTouchLongPressPortions() {
    return touchLongPressPortions;
}

/**
 * Set touch long press portions and save to NVRAM
 */
void setTouchLongPressPortions(uint8_t portions) {
    // Validate range
    if (portions < MIN_FOOD_PORTIONS) portions = MIN_FOOD_PORTIONS;
    if (portions > MAX_FOOD_PORTIONS) portions = MAX_FOOD_PORTIONS;
    
    touchLongPressPortions = portions;
    
    // Save to NVRAM
    touchPreferences.begin("touch", false);
    touchPreferences.putUChar(TOUCH_LONG_PRESS_PORTIONS_NVRAM_KEY, portions);
    touchPreferences.end();
    
    Console::printR(F("Touch long press portions set to: "));
    Console::printlnR(String(portions));
}

/**
 * Get current touch sensor enabled state
 */
bool getTouchSensorEnabled() {
    return touchSensorEnabled;
}

/**
 * Set touch sensor enabled/disabled state and save to NVRAM
 */
void setTouchSensorEnabled(bool enabled) {
    touchSensorEnabled = enabled;
    
    // Save to NVRAM
    touchPreferences.begin("touch", false);
    touchPreferences.putBool(TOUCH_SENSOR_ENABLED_NVRAM_KEY, enabled);
    touchPreferences.end();
    
    Console::printR(F("Touch sensor "));
    Console::printlnR(enabled ? F("ENABLED") : F("DISABLED"));
}

// ============================================================================
// MAIN LOOP
// ============================================================================

/**
 * Task: Process WiFi portal in non-blocking mode
 * Runs every 500ms to handle portal requests
 */
void wifiPortalTask() {
    wifiController.processConfigPortal();
}

void loop() {
  // Execute all scheduled tasks
  taskScheduler.execute();
}