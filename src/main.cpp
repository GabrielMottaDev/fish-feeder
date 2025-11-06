#include <Arduino.h>
#include <EEPROM.h>
#include <TaskScheduler.h>
#include "rtc_module.h"
#include "stepper_motor.h"
#include "feeding_controller.h"
#include "feeding_schedule.h"
#include "wifi_controller.h"
#include "ntp_sync.h"
#include "vibration_motor.h"
#include "rgb_led.h"
#include "config.h"
#include "console_manager.h"
#include "command_listener.h"

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

// Create feeding controller
FeedingController feedingController(&feedMotor);

// Create feeding schedule system
FeedingSchedule feedingSchedule;

// Create WiFi controller instance
WiFiController wifiController;

// Create NTP synchronization module
NTPSync ntpSync(&rtcModule, &wifiController);

// Global feeding state
bool isFeedingInProgress = false;

// Create command listener
CommandListener commandListener(&rtcModule, &feedMotor, &feedingController, 
                                &feedingSchedule, &wifiController, &ntpSync, 
                                &vibrationMotor, &rgbLed, &isFeedingInProgress);

// ============================================================================
// TASK SCHEDULER SETUP
// ============================================================================

// Create scheduler instance
Scheduler taskScheduler;

// Task callback functions
void displayTimeTask();
void processSerialTask();
void motorMaintenanceTask();
void vibrationMaintenanceTask();
void rgbLedMaintenanceTask();
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
 * Task: RGB LED maintenance
 * Runs every 20ms to handle timed operations, fading, and blinking
 */
void rgbLedMaintenanceTask() {
    rgbLed.update();
}

/**
 * Task: Monitor feeding operations
 * Runs every 100ms to check if async feeding is complete
 */
void feedingMonitorTask() {
    if (isFeedingInProgress && !feedMotor.isRunning()) {
        // Feeding completed - this is a response to a previous FEED command
        Console::printlnR(F("Food dispensing completed successfully"));
        isFeedingInProgress = false;
        tFeedingMonitor.disable(); // Stop monitoring until next feeding
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
    // Store previous connection state to detect changes
    static bool wasConnected = false;
    bool isConnected = wifiController.isWiFiConnected();
    
    // Check if WiFi just got connected
    if (isConnected && !wasConnected) {
        Console::printlnR(F("WiFi connection established - notifying NTP module"));
        ntpSync.onWiFiConnected();
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
    ntpSync.handleNTPSync();
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
    Console::printlnR(isFeedingInProgress ? F("Yes") : F("No"));
    
    Console::printR(F("Logging Enabled: "));
    Console::printlnR(ConsoleManager::isLoggingEnabled ? F("Yes") : F("No"));
    
    Console::printlnR(F("============================"));
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  
  // Wait for serial connection for debugging (non-blocking)
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < SERIAL_TIMEOUT)) {
    // Wait for serial connection with configurable timeout
  }
  
  Console::printlnR(F("=== Fish Feeder System Starting ==="));
  Console::printlnR(F("ESP32 - TaskScheduler-based Non-blocking Architecture"));
  
  // Initialize RTC module
  if (!rtcModule.begin()) {
    Console::printlnR(F("RTC initialization failed. System will continue with limited functionality."));
    Console::printlnR(F("Run 'rtcModule.scanI2C()' for manual diagnostics."));
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
  }
  
  // Initialize RGB LED
  if (!rgbLed.begin()) {
    Console::printlnR(F("ERROR: Failed to initialize RGB LED"));
    Console::printlnR(F("Check connections (ESP32 DevKit V1 30-pin):"));
    Console::printlnR(F("4-Pin RGB LED (Common Cathode/Anode):"));
    Console::printlnR(F("- Red   -> GPIO 25 -> 330Ω -> LED Red"));
    Console::printlnR(F("- Green -> GPIO 27 -> 330Ω -> LED Green"));
    Console::printlnR(F("- Blue  -> GPIO 32 -> 330Ω -> LED Blue"));
    Console::printlnR(F("- Common pin -> GND (cathode) or VCC (anode)"));
  } else {
    Console::printR(F("RGB LED: Initialized (GPIO 25/27/32) - "));
    Console::printlnR(RGB_LED_TYPE == 0 ? F("Common Cathode") : F("Common Anode"));
  }
  
  // Initialize WiFi Controller
  if (!wifiController.begin()) {
    Console::printlnR(F("WARNING: Failed to initialize WiFi Controller"));
    Console::printlnR(F("WiFi functions will be limited"));
  }
  
  // Configure WiFi Controller with FeedingSchedule reference for web interface
  wifiController.setFeedingSchedule(&feedingSchedule);
  wifiController.setFeedingController(&feedingController);
  
  // Initialize NTP Synchronization
  if (!ntpSync.begin()) {
    Console::printlnR(F("WARNING: Failed to initialize NTP synchronization"));
    Console::printlnR(F("Automatic time sync will not be available"));
  }
  
  // Initialize Feeding Schedule System
  feedingSchedule.begin(&feedingController, &rtcModule);
  // Note: Schedules are now loaded automatically from NVRAM in begin()
  // DEFAULT_FEEDING_SCHEDULE is only used on first boot or NVRAM reset
  Console::printlnR(F("Feeding Schedule: System initialized with persistent schedules"));
  
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
}

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