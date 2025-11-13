#include "command_listener.h"
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
#include "console_manager.h"

// Forward declarations for task control functions (implemented in main.cpp)
extern void pauseDisplayTask();
extern void resumeDisplayTask();
extern void pauseMotorTask();
extern void resumeMotorTask();
extern void showTaskStatus();
extern void enableFeedingMonitor();

// Forward declarations for centralized feeding operations (implemented in main.cpp)
extern bool startFeeding(uint8_t portions, bool recordInSchedule);
extern bool cancelFeeding();

/**
 * Constructor: Initialize command listener with ModuleManager
 */
CommandListener::CommandListener(ModuleManager* moduleManager)
    : modules(moduleManager) {
}

/**
 * Main command processing entry point
 */
bool CommandListener::processCommand(const String& command) {
    String cmd = command;
    cmd.trim();
    cmd.toUpperCase();
    
    // Process commands in order of priority/specificity
    if (processSystemCommands(cmd)) {
        return true;
    }
    if (processTaskCommands(cmd)) {
        return true;
    }
    if (processMotorCommands(cmd)) {
        return true;
    }
    if (processRTCCommands(cmd)) {
        return true;
    }
    if (processWiFiCommands(cmd)) {
        return true;
    }
    if (processNTPCommands(cmd)) {
        return true;
    }
    if (processScheduleCommands(cmd)) {
        return true;
    }
    if (processVibrationCommands(cmd)) {
        return true;
    }
    if (processRGBCommands(cmd)) {
        return true;
    }
    if (processTouchCommands(cmd)) {
        return true;
    }
    
    // Unknown command
    Console::printlnR(F("Unknown command. Type HELP for available commands."));
    return false;
}

/**
 * Process system-level commands (HELP, LOG, etc.)
 */
bool CommandListener::processSystemCommands(const String& command) {
    if (command == "HELP") {
        showHelp();
        return true;
    }
    else if (command == "LOG") {
        ConsoleManager::isLoggingEnabled = !ConsoleManager::isLoggingEnabled;
        Console::printR(F("Logging "));
        Console::printlnR(ConsoleManager::isLoggingEnabled ? F("ENABLED") : F("DISABLED"));
        return true;
    }
    else if (command == "INFO") {
        showSystemInfo();
        return true;
    }
    return false;
}

/**
 * Process task control commands
 */
bool CommandListener::processTaskCommands(const String& command) {
    if (command == "TASKS") {
        showTaskStatus();
        return true;
    }
    else if (command == "PAUSE DISPLAY") {
        pauseDisplayTask();
        Console::println(F("Display time task paused"));
        return true;
    }
    else if (command == "RESUME DISPLAY") {
        resumeDisplayTask();
        Console::println(F("Display time task resumed"));
        return true;
    }
    else if (command == "PAUSE MOTOR") {
        pauseMotorTask();
        Console::println(F("Motor maintenance task paused"));
        return true;
    }
    else if (command == "RESUME MOTOR") {
        resumeMotorTask();
        Console::println(F("Motor maintenance task resumed"));
        return true;
    }
    return false;
}

/**
 * Process motor and feeding commands
 */
bool CommandListener::processMotorCommands(const String& command) {
    if (command.startsWith("FEED")) {
        // Parse number of portions (default 1)
        int portions = 1;
        int spaceIndex = command.indexOf(' ');
        if (spaceIndex > 0) {
            String portionStr = command.substring(spaceIndex + 1);
            portions = portionStr.toInt();
            if (portions <= 0) portions = 1;
        }
        
        // Use centralized feeding method
        startFeeding(portions, true);
        return true;
    }
    else if (command == "CALIBRATE") {
        modules->getFeedingController()->calibrateFeeder();
        return true;
    }
    else if (command == "MOTOR STATUS") {
        modules->getStepperMotor()->printStatus();
        return true;
    }
    else if (command == "FEEDING STATUS") {
        modules->getFeedingController()->printFeedingStatus();
        return true;
    }
    else if (command == "CONFIG") {
        FeedingController::printFeedingConfiguration();
        return true;
    }
    else if (command.startsWith("STEP CW")) {
        int spaceIndex = command.lastIndexOf(' ');
        if (spaceIndex > 0) {
            String stepsStr = command.substring(spaceIndex + 1);
            int steps = stepsStr.toInt();
            if (steps > 0) {
                modules->getStepperMotor()->stepClockwise(steps);
                return true;
            }
        }
        Console::printlnR(F("Usage: STEP CW [steps]"));
        return true;
    }
    else if (command.startsWith("STEP CCW")) {
        int spaceIndex = command.lastIndexOf(' ');
        if (spaceIndex > 0) {
            String stepsStr = command.substring(spaceIndex + 1);
            int steps = stepsStr.toInt();
            if (steps > 0) {
                modules->getStepperMotor()->stepCounterClockwise(steps);
                return true;
            }
        }
        Console::printlnR(F("Usage: STEP CCW [steps]"));
        return true;
    }
    else if (command == "MOTOR HIGH PERFORMANCE") {
        modules->getStepperMotor()->enableHighPerformanceMode();
        return true;
    }
    else if (command == "MOTOR POWER SAVING") {
        modules->getStepperMotor()->enablePowerSavingMode();
        return true;
    }
    else if (command.startsWith("DIRECTION")) {
        int spaceIndex = command.indexOf(' ');
        if (spaceIndex > 0) {
            String directionStr = command.substring(spaceIndex + 1);
            directionStr.toUpperCase();
            
            if (directionStr == "CW" || directionStr == "CLOCKWISE") {
                modules->getStepperMotor()->setMotorDirection(true);
                Console::printlnR(F("Motor direction set to CLOCKWISE (CW)"));
                return true;
            }
            else if (directionStr == "CCW" || directionStr == "COUNTERCLOCKWISE" || directionStr == "COUNTER-CLOCKWISE") {
                modules->getStepperMotor()->setMotorDirection(false);
                Console::printlnR(F("Motor direction set to COUNTER-CLOCKWISE (CCW)"));
                return true;
            }
        }
        
        // Show current direction and usage
        Console::printR(F("Current direction: "));
        Console::printlnR(modules->getStepperMotor()->getMotorDirection() ? F("CLOCKWISE (CW)") : F("COUNTER-CLOCKWISE (CCW)"));
        Console::printlnR(F("Usage: DIRECTION [CW|CCW]"));
        return true;
    }
    return false;
}

/**
 * Process RTC-related commands
 */
bool CommandListener::processRTCCommands(const String& command) {
    if (command == "TIME") {
        modules->getRTCModule()->printDateTime();
        return true;
    }
    else if (command.startsWith("SET ")) {
        // Use RTCModule's built-in command processing for SET commands
        return modules->getRTCModule()->processCommand(command);
    }
    return false;
}

/**
 * Display comprehensive help information organized by categories
 */
void CommandListener::showHelp() {
    Console::printlnR(F(""));
    Console::printlnR(F("=== FISH FEEDER SYSTEM HELP ==="));
    Console::printlnR(F(""));
    
    Console::printlnR(F("SYSTEM COMMANDS:"));
    Console::printlnR(F("  HELP                    - Show this help message"));
    Console::printlnR(F("  INFO                    - Show system information"));
    Console::printlnR(F("  LOG                     - Toggle logging output"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("TASK CONTROL:"));
    Console::printlnR(F("  TASKS                   - Show task scheduler status"));
    Console::printlnR(F("  PAUSE DISPLAY           - Pause time display"));
    Console::printlnR(F("  RESUME DISPLAY          - Resume time display"));
    Console::printlnR(F("  PAUSE MOTOR             - Pause motor maintenance"));
    Console::printlnR(F("  RESUME MOTOR            - Resume motor maintenance"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("MOTOR & FEEDING:"));
    Console::printR(F("  FEED [portions]         - Dispense food ("));
    Console::printR(String(MIN_FOOD_PORTIONS));
    Console::printR(F("-"));
    Console::printR(String(MAX_FOOD_PORTIONS));
    Console::printlnR(F(" portions)"));
    Console::printlnR(F("  CALIBRATE               - Full feeder calibration"));
    Console::printlnR(F("  MOTOR STATUS            - Show motor information"));
    Console::printlnR(F("  FEEDING STATUS          - Show feeding system status"));
    Console::printlnR(F("  CONFIG                  - Show feeding configuration"));
    Console::printlnR(F("  STEP CW [steps]         - Step clockwise"));
    Console::printlnR(F("  STEP CCW [steps]        - Step counter-clockwise"));
    Console::printlnR(F("  DIRECTION [CW|CCW]      - Set/show motor rotation direction"));
    Console::printlnR(F("  MOTOR HIGH PERFORMANCE  - Enable max speed/torque mode"));
    Console::printlnR(F("  MOTOR POWER SAVING      - Enable power-efficient mode"));    
    Console::printlnR(F(""));
    
    Console::printlnR(F("RTC COMMANDS:"));
    Console::printlnR(F("  TIME                    - Show current date and time"));
    Console::printlnR(F("  SET DD/MM/YYYY HH:MM:SS - Set date and time"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("WIFI COMMANDS:"));
    Console::printlnR(F("  WIFI SCAN               - Scan for available networks"));
    Console::printlnR(F("  WIFI CONNECT SSID PASS  - Connect to network with password"));
    Console::printlnR(F("  WIFI CONNECT SSID       - Connect to saved network"));
    Console::printlnR(F("  WIFI DISCONNECT         - Disconnect from current network"));
    Console::printlnR(F("  WIFI STATUS             - Show WiFi connection status"));
    Console::printlnR(F("  WIFI TEST               - Test internet connectivity"));
    Console::printlnR(F("  WIFI DNS CONFIG         - Configure DNS servers"));
    Console::printlnR(F("  WIFI DNS TEST           - Test all DNS servers"));
    Console::printlnR(F("  WIFI LIST               - List saved networks"));
    Console::printlnR(F("  WIFI REMOVE SSID        - Remove saved network"));
    Console::printlnR(F("  WIFI CLEAR              - Clear all saved networks"));
    Console::printlnR(F("  WIFI PORTAL [name]      - Start configuration web portal"));
    Console::printlnR(F("  WIFI PORTAL START       - Restart always-on portal"));
    Console::printlnR(F("  WIFI PORTAL STOP        - Stop configuration portal"));
    Console::printlnR(F("  WIFI CONFIG             - Show WiFi portal configuration"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("NTP TIME SYNC:"));
    Console::printlnR(F("  NTP STATUS              - Show NTP synchronization status"));
    Console::printlnR(F("  NTP STATS               - Show NTP synchronization statistics"));
    Console::printlnR(F("  NTP SYNC                - Force immediate NTP synchronization"));
    Console::printlnR(F("  NTP FALLBACK            - Force HTTP time fallback test"));
    Console::printlnR(F("  NTP INTERVAL [minutes]  - Set sync interval in minutes"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("FEEDING SCHEDULE:"));
    Console::printlnR(F("  SCHEDULE STATUS         - Show schedule system status"));
    Console::printlnR(F("  SCHEDULE LIST           - List all configured schedules"));
    Console::printlnR(F("  SCHEDULE NEXT           - Show next feeding time"));
    Console::printlnR(F("  SCHEDULE LAST           - Show last feeding"));
    Console::printlnR(F("  SCHEDULE ENABLE         - Enable schedule system"));
    Console::printlnR(F("  SCHEDULE DISABLE        - Disable schedule system"));
    Console::printlnR(F("  SCHEDULE ENABLE n       - Enable schedule n"));
    Console::printlnR(F("  SCHEDULE DISABLE n      - Disable schedule n"));
    Console::printlnR(F("  SCHEDULE TOLERANCE mins - Set missed feeding tolerance"));
    Console::printlnR(F("  SCHEDULE RECOVERY hrs   - Set recovery period"));
    Console::printlnR(F("  SCHEDULE DIAGNOSTICS    - Show diagnostics"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("VIBRATION MOTOR:"));
    Console::printlnR(F("  VIB STATUS              - Show vibration status"));
    Console::printlnR(F("  VIB ON [intensity]      - Start continuous (0-100%, default 50%)"));
    Console::printlnR(F("  VIB STOP                - Stop vibration"));
    Console::printlnR(F("  VIB TIMED <int> <ms>    - Timed vibration (intensity, duration)"));
    Console::printlnR(F("  VIB SET <intensity>     - Change intensity (0-100%)"));
    Console::printlnR(F("  VIB TEST                - Quick test pulse"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("RGB LED:"));
    Console::printlnR(F("  RGB STATUS              - Show LED status"));
    Console::printlnR(F("  RGB ON [ms]             - Turn on (instant or fade)"));
    Console::printlnR(F("  RGB OFF [ms]            - Turn off (instant or fade)"));
    Console::printlnR(F("  RGB COLOR <r> <g> <b>   - Set custom color (0-255)"));
    Console::printlnR(F("  RGB [color]             - Predefined colors:"));
    Console::printlnR(F("                            RED, GREEN, BLUE, YELLOW, CYAN,"));
    Console::printlnR(F("                            MAGENTA, WHITE, ORANGE, PURPLE"));
    Console::printlnR(F("  RGB BRIGHTNESS <0-100>  - Set brightness %"));
    Console::printlnR(F("  RGB TIMED <ms>          - On for duration"));
    Console::printlnR(F("  RGB FADE <color> <ms>   - Fade to color name"));
    Console::printlnR(F("  RGB FADE <r> <g> <b> <ms> - Fade to RGB color"));
    Console::printlnR(F("  RGB BLINK <int> [cnt]   - Blink LED (interval, count)"));
    Console::printlnR(F("  RGB STOPBLINK           - Stop blinking"));
    Console::printlnR(F("  RGB TEST                - Run test sequence"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("TOUCH SENSOR:"));
    Console::printlnR(F("  TOUCH STATUS            - Show sensor status"));
    Console::printlnR(F("  TOUCH RESET             - Reset statistics"));
    Console::printlnR(F("  TOUCH DEBOUNCE <ms>     - Set debounce delay"));
    Console::printlnR(F("  TOUCH LONGPRESS <ms>    - Set long press duration"));
    Console::printlnR(F("  TOUCH LONGPRESS ENABLE  - Enable long press"));
    Console::printlnR(F("  TOUCH LONGPRESS DISABLE - Disable long press"));
    Console::printlnR(F("  TOUCH TEST              - Test touch detection"));
    Console::printlnR(F(""));
    
    Console::printlnR(F("EXAMPLES:"));
    Console::printlnR(F("  FEED 3                  - Dispense 3 portions"));
    Console::printlnR(F("  SET 29/10/2025 14:30:00 - Set date/time"));
    Console::printlnR(F("  STEP CW 100             - Step 100 steps clockwise"));
    Console::printlnR(F("  WIFI CONNECT MyWiFi password123 - Connect to WiFi"));
    Console::printlnR(F("  WIFI TEST               - Test internet connectivity"));
    Console::printlnR(F("  WIFI PORTAL MyFeeder    - Start config portal as MyFeeder"));
    Console::printlnR(F("  NTP SYNC                - Sync time with internet"));
    Console::printlnR(F("  NTP INTERVAL 60         - Set sync every 60 minutes"));
    Console::printlnR(F("  SCHEDULE STATUS         - Show schedule status"));
    Console::printlnR(F("  SCHEDULE DISABLE 1      - Disable schedule 1"));
    Console::printlnR(F("  SCHEDULE TOLERANCE 45   - Allow 45 minutes tolerance"));
    Console::printlnR(F("  VIB ON 75               - Start vibration at 75%"));
    Console::printlnR(F("  VIB TIMED 100 1500      - Vibrate 100% for 1.5 seconds"));
    Console::printlnR(F("  RGB RED                 - Set color to red"));
    Console::printlnR(F("  RGB ON 2000             - Fade on over 2 seconds"));
    Console::printlnR(F("  RGB OFF 1500            - Fade off over 1.5 seconds"));
    Console::printlnR(F("  RGB COLOR 255 128 0     - Set color to orange"));
    Console::printlnR(F("  RGB BRIGHTNESS 50       - Set 50% brightness"));
    Console::printlnR(F("  RGB TIMED 3000          - On for 3 seconds"));
    Console::printlnR(F("  RGB FADE RED 2000       - Fade to red in 2 seconds"));
    Console::printlnR(F("  RGB FADE 0 0 255 2000   - Fade to blue in 2 seconds"));
    Console::printlnR(F("  RGB BLINK 500 10        - Blink 10 times, 500ms interval"));
    Console::printlnR(F("  TOUCH STATUS            - Show touch sensor status"));
    Console::printlnR(F("  TOUCH DEBOUNCE 40       - Set 40ms debounce"));
    Console::printlnR(F("  TOUCH LONGPRESS 1500    - Set 1.5s long press"));
    Console::printlnR(F(""));
    Console::printlnR(F("==============================="));
}

/**
 * Display system information and status
 */
void CommandListener::showSystemInfo() {
    Console::printlnR(F(""));
    Console::printlnR(F("=== FISH FEEDER SYSTEM INFO ==="));
    Console::printR(F("System: "));
    Console::printlnR(F("TaskScheduler-based Non-blocking Architecture"));
    Console::printR(F("Logging: "));
    Console::printlnR(ConsoleManager::isLoggingEnabled ? F("ENABLED") : F("DISABLED"));
    Console::printR(F("RTC Status: "));
    Console::printlnR(modules && modules->hasRTCModule() ? F("Connected") : F("Not Available"));
    Console::printR(F("Motor Status: "));
    Console::printlnR(modules->getStepperMotor()->isReady() ? F("Ready") : F("Not Ready"));
    Console::printR(F("Feeding Controller: "));
    Console::printlnR(modules->getFeedingController()->isReady() ? F("Ready") : F("Not Ready"));
    Console::printR(F("Feeding in Progress: "));
    Console::printlnR(modules->getFeedingInProgress() ? F("Yes") : F("No"));
    Console::printR(F("WiFi Status: "));
    Console::printlnR(modules->getWiFiController()->isWiFiConnected() ? F("Connected") : F("Disconnected"));
    Console::printR(F("Config Portal: "));
    Console::printlnR(modules->getWiFiController()->isConfigPortalActive() ? F("Active") : F("Inactive"));
    Console::printlnR(F("=============================="));
}

/**
 * Process WiFi commands
 */
bool CommandListener::processWiFiCommands(const String& command) {
    if (command == "WIFI CONFIG") {
        showWiFiPortalConfig();
        return true;
    }
    // WiFi commands
    else if (command.startsWith("WIFI ")) {
        return modules->getWiFiController()->processWiFiCommand(command);
    }
    
    return false;
}

/**
 * Show WiFi Portal Configuration
 */
void CommandListener::showWiFiPortalConfig() {
    Console::printlnR(F(""));
    Console::printlnR(F("=== WIFI PORTAL CONFIGURATION ==="));
    Console::printR(F("Auto-start on boot: "));
    Console::printlnR(WIFI_PORTAL_AUTO_START ? F("ENABLED") : F("DISABLED"));
    Console::printR(F("Auto-start on disconnect: "));
    Console::printlnR(WIFI_PORTAL_ON_DISCONNECT ? F("ENABLED") : F("DISABLED"));
    Console::printR(F("Portal timeout: "));
    Console::printR(String(WIFI_PORTAL_TIMEOUT / 60000));
    Console::printlnR(F(" minutes"));
    Console::printR(F("Access Point name: "));
    Console::printlnR(WIFI_PORTAL_AP_NAME);
    Console::printR(F("Access Point password: "));
    if (strlen(WIFI_PORTAL_AP_PASSWORD) > 0) {
        Console::printlnR(WIFI_PORTAL_AP_PASSWORD);
    } else {
        Console::printlnR(F("(no password - open network)"));
    }
    Console::printlnR(F("Connection check interval: "));
    Console::printR(String(WIFI_CONNECTION_CHECK_INTERVAL / 1000));
    Console::printlnR(F(" seconds"));
    Console::printlnR(F("================================="));
}

/**
 * Process NTP time synchronization commands
 */
bool CommandListener::processNTPCommands(const String& command) {
    if (command.startsWith("NTP ")) {
        return modules->getNTPSync()->processNTPCommand(command);
    }
    
    return false;
}

/**
 * Process feeding schedule commands
 */
bool CommandListener::processScheduleCommands(const String& command) {
    if (!command.startsWith("SCHEDULE ")) {
        return false;
    }
    
    String subCommand = command.substring(9); // Remove "SCHEDULE "
    subCommand.trim();
    
    if (subCommand == "STATUS") {
        modules->getFeedingSchedule()->printScheduleStatus();
        return true;
    }
    
    if (subCommand == "LIST") {
        modules->getFeedingSchedule()->printScheduleList();
        return true;
    }
    
    if (subCommand == "NEXT") {
        modules->getFeedingSchedule()->printNextFeeding();
        return true;
    }
    
    if (subCommand == "LAST") {
        modules->getFeedingSchedule()->printLastFeeding();
        return true;
    }
    
    if (subCommand == "ENABLE") {
        modules->getFeedingSchedule()->enableSchedule(true);
        return true;
    }
    
    if (subCommand == "DISABLE") {
        modules->getFeedingSchedule()->enableSchedule(false);
        return true;
    }
    
    if (subCommand.startsWith("ENABLE ")) {
        String indexStr = subCommand.substring(7);
        int index = indexStr.toInt();
        if (index >= 0 && index < modules->getFeedingSchedule()->getScheduleCount()) {
            modules->getFeedingSchedule()->enableScheduleAtIndex(index, true);
        } else {
            Console::printlnR(F("ERROR: Invalid schedule index"));
        }
        return true;
    }
    
    if (subCommand.startsWith("DISABLE ")) {
        String indexStr = subCommand.substring(8);
        int index = indexStr.toInt();
        if (index >= 0 && index < modules->getFeedingSchedule()->getScheduleCount()) {
            modules->getFeedingSchedule()->enableScheduleAtIndex(index, false);
        } else {
            Console::printlnR(F("ERROR: Invalid schedule index"));
        }
        return true;
    }
    
    if (subCommand.startsWith("TOLERANCE ")) {
        String toleranceStr = subCommand.substring(10);
        int tolerance = toleranceStr.toInt();
        if (tolerance > 0 && tolerance <= 120) { // Max 2 hours
            modules->getFeedingSchedule()->setTolerance(tolerance);
        } else {
            Console::printlnR(F("ERROR: Tolerance must be 1-120 minutes"));
        }
        return true;
    }
    
    if (subCommand.startsWith("RECOVERY ")) {
        String recoveryStr = subCommand.substring(9);
        int recovery = recoveryStr.toInt();
        if (recovery > 0 && recovery <= 72) { // Max 72 hours
            modules->getFeedingSchedule()->setMaxRecoveryHours(recovery);
        } else {
            Console::printlnR(F("ERROR: Recovery must be 1-72 hours"));
        }
        return true;
    }
    
    if (subCommand == "DIAGNOSTICS") {
        modules->getFeedingSchedule()->printDiagnostics();
        return true;
    }
    
    if (subCommand == "TEST") {
        modules->getFeedingSchedule()->testScheduleCalculation();
        return true;
    }
    
    // Unknown schedule command
    Console::printlnR(F("Unknown SCHEDULE command. Available:"));
    Console::printlnR(F("  SCHEDULE STATUS    - Show schedule system status"));
    Console::printlnR(F("  SCHEDULE LIST      - List all schedules"));
    Console::printlnR(F("  SCHEDULE NEXT      - Show next feeding time"));
    Console::printlnR(F("  SCHEDULE LAST      - Show last feeding"));
    Console::printlnR(F("  SCHEDULE ENABLE    - Enable schedule system"));
    Console::printlnR(F("  SCHEDULE DISABLE   - Disable schedule system"));
    Console::printlnR(F("  SCHEDULE ENABLE n  - Enable schedule n"));
    Console::printlnR(F("  SCHEDULE DISABLE n - Disable schedule n"));
    Console::printlnR(F("  SCHEDULE TOLERANCE mins - Set tolerance (1-120)"));
    Console::printlnR(F("  SCHEDULE RECOVERY hrs   - Set recovery period (1-72)"));
    Console::printlnR(F("  SCHEDULE DIAGNOSTICS    - Show diagnostics"));
    Console::printlnR(F("  SCHEDULE TEST           - Test schedule calculation"));
    
    return true;
}

// ============================================================================
// VIBRATION MOTOR COMMANDS
// ============================================================================

/**
 * Process vibration motor commands
 */
bool CommandListener::processVibrationCommands(const String& command) {
    if (!command.startsWith("VIB")) {
        return false;
    }
    
    // VIB STATUS - Show vibration motor status
    if (command == "VIB STATUS") {
        Console::printlnR(modules->getVibrationMotor()->getStatus());
        return true;
    }
    
    // VIB STOP - Stop vibration
    if (command == "VIB STOP") {
        modules->getVibrationMotor()->stop();
        Console::printlnR(F("Vibration stopped"));
        return true;
    }
    
    // VIB ON [intensity] - Start continuous vibration
    if (command.startsWith("VIB ON")) {
        int intensity = 50;
        
        int spacePos = command.indexOf(' ', 7);
        if (spacePos > 0) {
            String intensityStr = command.substring(spacePos + 1);
            intensity = intensityStr.toInt();
        }
        
        if (intensity < 0 || intensity > 100) {
            Console::printlnR(F("ERROR: Intensity must be 0-100%"));
            return true;
        }
        
        modules->getVibrationMotor()->startContinuous(intensity);
        Console::printR(F("Vibration started at "));
        Console::printR(String(intensity));
        Console::printlnR(F("% intensity"));
        return true;
    }
    
    // VIB TIMED [intensity] [duration_ms] - Timed vibration
    if (command.startsWith("VIB TIMED ")) {
        // Remove "VIB TIMED " prefix (10 characters)
        String params = command.substring(10);
        params.trim();
        
        // Find space between parameters
        int spacePos = params.indexOf(' ');
        if (spacePos < 0) {
            Console::printlnR(F("Usage: VIB TIMED <intensity> <duration_ms>"));
            Console::printlnR(F("Example: VIB TIMED 75 2000 (75% for 2 seconds)"));
            return true;
        }
        
        String intensityStr = params.substring(0, spacePos);
        String durationStr = params.substring(spacePos + 1);
        durationStr.trim();
        
        int intensity = intensityStr.toInt();
        unsigned long duration = durationStr.toInt();
        
        if (intensity < 0 || intensity > 100) {
            Console::printlnR(F("ERROR: Intensity must be 0-100%"));
            return true;
        }
        
        if (duration == 0) {
            Console::printlnR(F("ERROR: Duration must be > 0 milliseconds"));
            return true;
        }
        
        modules->getVibrationMotor()->startTimed(intensity, duration);
        Console::printR(F("Vibration: "));
        Console::printR(String(intensity));
        Console::printR(F("% for "));
        Console::printR(String(duration));
        Console::printlnR(F("ms"));
        return true;
    }
    
    // VIB SET [intensity] - Change intensity during vibration
    if (command.startsWith("VIB SET")) {
        int spacePos = command.indexOf(' ', 8);
        if (spacePos < 0) {
            Console::printlnR(F("Usage: VIB SET <intensity>"));
            return true;
        }
        
        String intensityStr = command.substring(spacePos + 1);
        int intensity = intensityStr.toInt();
        
        if (intensity < 0 || intensity > 100) {
            Console::printlnR(F("ERROR: Intensity must be 0-100%"));
            return true;
        }
        
        modules->getVibrationMotor()->setIntensity(intensity);
        Console::printR(F("Intensity set to "));
        Console::printR(String(intensity));
        Console::printlnR(F("%"));
        return true;
    }
    
    // VIB TEST - Quick test vibration
    if (command == "VIB TEST") {
        Console::printlnR(F("Running vibration test..."));
        modules->getVibrationMotor()->startTimed(100, 200);
        return true;
    }
    
    // Unknown vibration command
    Console::printlnR(F("Unknown VIB command. Available:"));
    Console::printlnR(F("  VIB STATUS             - Show vibration status"));
    Console::printlnR(F("  VIB ON [intensity]     - Start continuous (default 50%)"));
    Console::printlnR(F("  VIB STOP               - Stop vibration"));
    Console::printlnR(F("  VIB TIMED <int> <ms>   - Timed vibration"));
    Console::printlnR(F("  VIB SET <intensity>    - Change intensity (0-100%)"));
    Console::printlnR(F("  VIB TEST               - Quick test pulse"));
    Console::printlnR(F("Examples:"));
    Console::printlnR(F("  VIB ON 75              - 75% continuous"));
    Console::printlnR(F("  VIB TIMED 100 1500     - 100% for 1.5 seconds"));
    return true;
}

/**
 * Process RGB LED commands
 */
bool CommandListener::processRGBCommands(const String& command) {
    if (!command.startsWith("RGB")) {
        return false;
    }
    
    // RGB STATUS - Show RGB LED status
    if (command == "RGB STATUS") {
        Console::printlnR(modules->getRGBLed()->getStatus());
        return true;
    }
    
    // RGB ON [duration] - Turn LED on (instant or fade)
    if (command.startsWith("RGB ON")) {
        String params = command.substring(6);
        params.trim();
        
        if (params.length() == 0) {
            // Instant on
            modules->getRGBLed()->turnOn();
            Console::printlnR(F("RGB LED turned on"));
        } else {
            // Fade on over duration
            unsigned long duration = params.toInt();
            if (duration == 0) {
                Console::printlnR(F("ERROR: Duration must be > 0"));
                return true;
            }
            
            // Get current color and fade from black to that color
            RGBLed::Color currentColor = modules->getRGBLed()->getColor();
            RGBLed::Color black = {0, 0, 0};
            
            // Set to black first (without turning on)
            modules->getRGBLed()->setColor(black);
            modules->getRGBLed()->turnOn();
            
            // Fade to original color
            modules->getRGBLed()->fadeTo(currentColor, duration);
            
            Console::printR(F("Fading on over "));
            Console::printR(String(duration));
            Console::printlnR(F("ms"));
        }
        return true;
    }
    
    // RGB OFF [duration] - Turn LED off (instant or fade)
    if (command.startsWith("RGB OFF")) {
        String params = command.substring(7);
        params.trim();
        
        if (params.length() == 0) {
            // Instant off
            modules->getRGBLed()->stopBlink();  // Explicitly stop blink
            modules->getRGBLed()->turnOff();
            Console::printlnR(F("RGB LED turned off"));
        } else {
            // Fade off over duration
            unsigned long duration = params.toInt();
            if (duration == 0) {
                Console::printlnR(F("ERROR: Duration must be > 0"));
                return true;
            }
            
            modules->getRGBLed()->stopBlink();  // Stop blink before fading
            
            // Fade to black
            RGBLed::Color black = {0, 0, 0};
            modules->getRGBLed()->fadeTo(black, duration);
            
            Console::printR(F("Fading off over "));
            Console::printR(String(duration));
            Console::printlnR(F("ms"));
        }
        return true;
    }
    
    // RGB COLOR [r] [g] [b] - Set custom RGB color
    if (command.startsWith("RGB COLOR")) {
        String params = command.substring(10);
        params.trim();
        
        int space1 = params.indexOf(' ');
        int space2 = params.lastIndexOf(' ');
        
        if (space1 < 0 || space2 < 0 || space1 == space2) {
            Console::printlnR(F("Usage: RGB COLOR <red> <green> <blue>"));
            Console::printlnR(F("  Values: 0-255 for each channel"));
            Console::printlnR(F("Examples:"));
            Console::printlnR(F("  RGB COLOR 255 0 0      - Red"));
            Console::printlnR(F("  RGB COLOR 0 255 0      - Green"));
            Console::printlnR(F("  RGB COLOR 255 255 0    - Yellow"));
            return true;
        }
        
        String rStr = params.substring(0, space1);
        String gStr = params.substring(space1 + 1, space2);
        String bStr = params.substring(space2 + 1);
        
        int r = rStr.toInt();
        int g = gStr.toInt();
        int b = bStr.toInt();
        
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            Console::printlnR(F("ERROR: Values must be 0-255"));
            return true;
        }
        
        modules->getRGBLed()->setColor(r, g, b);
        Console::printR(F("Color set to RGB("));
        Console::printR(String(r));
        Console::printR(F(", "));
        Console::printR(String(g));
        Console::printR(F(", "));
        Console::printR(String(b));
        Console::printlnR(F(")"));
        return true;
    }
    
    // RGB RED/GREEN/BLUE/YELLOW/CYAN/MAGENTA/WHITE/ORANGE/PURPLE - Predefined colors
    if (command == "RGB RED") {
        modules->getRGBLed()->setColor(RGBLed::RED);
        Console::printlnR(F("Color: Red"));
        return true;
    }
    if (command == "RGB GREEN") {
        modules->getRGBLed()->setColor(RGBLed::GREEN);
        Console::printlnR(F("Color: Green"));
        return true;
    }
    if (command == "RGB BLUE") {
        modules->getRGBLed()->setColor(RGBLed::BLUE);
        Console::printlnR(F("Color: Blue"));
        return true;
    }
    if (command == "RGB YELLOW") {
        modules->getRGBLed()->setColor(RGBLed::YELLOW);
        Console::printlnR(F("Color: Yellow"));
        return true;
    }
    if (command == "RGB CYAN") {
        modules->getRGBLed()->setColor(RGBLed::CYAN);
        Console::printlnR(F("Color: Cyan"));
        return true;
    }
    if (command == "RGB MAGENTA") {
        modules->getRGBLed()->setColor(RGBLed::MAGENTA);
        Console::printlnR(F("Color: Magenta"));
        return true;
    }
    if (command == "RGB WHITE") {
        modules->getRGBLed()->setColor(RGBLed::WHITE);
        Console::printlnR(F("Color: White"));
        return true;
    }
    if (command == "RGB ORANGE") {
        modules->getRGBLed()->setColor(RGBLed::ORANGE);
        Console::printlnR(F("Color: Orange"));
        return true;
    }
    if (command == "RGB PURPLE") {
        modules->getRGBLed()->setColor(RGBLed::PURPLE);
        Console::printlnR(F("Color: Purple"));
        return true;
    }
    
    // RGB FADE [color] [duration] - Fade to predefined color
    if (command.startsWith("RGB FADE ")) {
        String params = command.substring(9);
        params.trim();
        
        int spacePos = params.indexOf(' ');
        if (spacePos < 0) {
            Console::printlnR(F("Usage: RGB FADE <color> <duration_ms>"));
            Console::printlnR(F("Colors: RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, ORANGE, PURPLE"));
            Console::printlnR(F("Example: RGB FADE RED 2000 - Fade to red in 2s"));
            return true;
        }
        
        String colorName = params.substring(0, spacePos);
        String durationStr = params.substring(spacePos + 1);
        colorName.trim();
        durationStr.trim();
        
        unsigned long duration = durationStr.toInt();
        if (duration == 0) {
            Console::printlnR(F("ERROR: Duration must be > 0"));
            return true;
        }
        
        RGBLed::Color targetColor;
        bool validColor = false;
        
        if (colorName == "RED") {
            targetColor = RGBLed::RED;
            validColor = true;
        } else if (colorName == "GREEN") {
            targetColor = RGBLed::GREEN;
            validColor = true;
        } else if (colorName == "BLUE") {
            targetColor = RGBLed::BLUE;
            validColor = true;
        } else if (colorName == "YELLOW") {
            targetColor = RGBLed::YELLOW;
            validColor = true;
        } else if (colorName == "CYAN") {
            targetColor = RGBLed::CYAN;
            validColor = true;
        } else if (colorName == "MAGENTA") {
            targetColor = RGBLed::MAGENTA;
            validColor = true;
        } else if (colorName == "WHITE") {
            targetColor = RGBLed::WHITE;
            validColor = true;
        } else if (colorName == "ORANGE") {
            targetColor = RGBLed::ORANGE;
            validColor = true;
        } else if (colorName == "PURPLE") {
            targetColor = RGBLed::PURPLE;
            validColor = true;
        }
        
        if (!validColor) {
            Console::printlnR(F("ERROR: Invalid color name"));
            Console::printlnR(F("Valid colors: RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, ORANGE, PURPLE"));
            return true;
        }
        
        modules->getRGBLed()->fadeTo(targetColor, duration);
        Console::printR(F("Fading to "));
        Console::printR(colorName);
        Console::printR(F(" in "));
        Console::printR(String(duration));
        Console::printlnR(F("ms"));
        return true;
    }
    
    // RGB BRIGHTNESS [0-100] - Set brightness
    if (command.startsWith("RGB BRIGHTNESS")) {
        String brightnessStr = command.substring(15);
        brightnessStr.trim();
        
        if (brightnessStr.length() == 0) {
            Console::printlnR(F("Usage: RGB BRIGHTNESS <0-100>"));
            return true;
        }
        
        int brightness = brightnessStr.toInt();
        
        if (brightness < 0 || brightness > 100) {
            Console::printlnR(F("ERROR: Brightness must be 0-100%"));
            return true;
        }
        
        modules->getRGBLed()->setBrightness(brightness);
        Console::printR(F("Brightness: "));
        Console::printR(String(brightness));
        Console::printlnR(F("%"));
        return true;
    }
    
    // RGB TIMED [duration] - Turn on for duration (ms)
    if (command.startsWith("RGB TIMED")) {
        String durationStr = command.substring(10);
        durationStr.trim();
        
        if (durationStr.length() == 0) {
            Console::printlnR(F("Usage: RGB TIMED <milliseconds>"));
            Console::printlnR(F("Example: RGB TIMED 3000 - On for 3 seconds"));
            return true;
        }
        
        unsigned long duration = durationStr.toInt();
        
        if (duration == 0) {
            Console::printlnR(F("ERROR: Duration must be > 0"));
            return true;
        }
        
        modules->getRGBLed()->turnOnFor(duration);
        Console::printR(F("LED on for "));
        Console::printR(String(duration));
        Console::printlnR(F("ms"));
        return true;
    }
    
    // RGB FADE [r] [g] [b] [duration] - Fade to color
    if (command.startsWith("RGB FADE")) {
        String params = command.substring(9);
        params.trim();
        
        int space1 = params.indexOf(' ');
        int space2 = params.indexOf(' ', space1 + 1);
        int space3 = params.lastIndexOf(' ');
        
        if (space1 < 0 || space2 < 0 || space3 < 0 || space2 == space3) {
            Console::printlnR(F("Usage: RGB FADE <r> <g> <b> <duration_ms>"));
            Console::printlnR(F("Example: RGB FADE 255 0 0 2000 - Fade to red in 2s"));
            return true;
        }
        
        String rStr = params.substring(0, space1);
        String gStr = params.substring(space1 + 1, space2);
        String bStr = params.substring(space2 + 1, space3);
        String durationStr = params.substring(space3 + 1);
        
        int r = rStr.toInt();
        int g = gStr.toInt();
        int b = bStr.toInt();
        unsigned long duration = durationStr.toInt();
        
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            Console::printlnR(F("ERROR: RGB values must be 0-255"));
            return true;
        }
        
        if (duration == 0) {
            Console::printlnR(F("ERROR: Duration must be > 0"));
            return true;
        }
        
        RGBLed::Color targetColor = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
        modules->getRGBLed()->fadeTo(targetColor, duration);
        Console::printlnR(F("Fading to new color..."));
        return true;
    }
    
    // RGB BLINK [interval] [count] - Blink LED
    if (command.startsWith("RGB BLINK")) {
        String params = command.substring(10);
        params.trim();
        
        int spacePos = params.indexOf(' ');
        String intervalStr;
        String countStr;
        
        if (spacePos < 0) {
            intervalStr = params;
            countStr = "0";  // Infinite
        } else {
            intervalStr = params.substring(0, spacePos);
            countStr = params.substring(spacePos + 1);
        }
        
        if (intervalStr.length() == 0) {
            Console::printlnR(F("Usage: RGB BLINK <interval_ms> [count]"));
            Console::printlnR(F("  count = 0 for infinite blinking"));
            Console::printlnR(F("Examples:"));
            Console::printlnR(F("  RGB BLINK 500       - Blink every 500ms (infinite)"));
            Console::printlnR(F("  RGB BLINK 1000 5    - Blink 5 times, 1s interval"));
            return true;
        }
        
        unsigned long interval = intervalStr.toInt();
        uint16_t count = countStr.toInt();
        
        if (interval == 0) {
            Console::printlnR(F("ERROR: Interval must be > 0"));
            return true;
        }
        
        modules->getRGBLed()->blink(interval, count);
        Console::printR(F("Blinking: "));
        Console::printR(String(interval));
        Console::printR(F("ms, "));
        if (count == 0) {
            Console::printlnR(F("infinite"));
        } else {
            Console::printR(String(count));
            Console::printlnR(F(" times"));
        }
        return true;
    }
    
    // RGB STOPBLINK - Stop blinking
    if (command == "RGB STOPBLINK") {
        modules->getRGBLed()->stopBlink();
        Console::printlnR(F("Blinking stopped"));
        return true;
    }
    
    // RGB TEST - Quick test sequence
    if (command == "RGB TEST") {
        Console::printlnR(F("RGB LED Test Sequence:"));
        Console::printlnR(F("  Red → Green → Blue → Off"));
        
        modules->getRGBLed()->setColor(RGBLed::RED);
        modules->getRGBLed()->turnOnFor(1000);
        delay(1000);
        
        modules->getRGBLed()->setColor(RGBLed::GREEN);
        modules->getRGBLed()->turnOnFor(1000);
        delay(1000);
        
        modules->getRGBLed()->setColor(RGBLed::BLUE);
        modules->getRGBLed()->turnOnFor(1000);
        
        Console::printlnR(F("Test complete!"));
        return true;
    }
    
    // Unknown RGB command
    Console::printlnR(F("Unknown RGB command. Available:"));
    Console::printlnR(F("  RGB STATUS                  - Show LED status"));
    Console::printlnR(F("  RGB ON [ms]                 - Turn on (instant or fade)"));
    Console::printlnR(F("  RGB OFF [ms]                - Turn off (instant or fade)"));
    Console::printlnR(F("  RGB COLOR <r> <g> <b>       - Set custom color (0-255)"));
    Console::printlnR(F("  RGB [color name]            - Predefined colors"));
    Console::printlnR(F("    Names: RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA,"));
    Console::printlnR(F("           WHITE, ORANGE, PURPLE"));
    Console::printlnR(F("  RGB BRIGHTNESS <0-100>      - Set brightness %"));
    Console::printlnR(F("  RGB TIMED <ms>              - On for duration"));
    Console::printlnR(F("  RGB FADE <color> <ms>       - Fade to color name"));
    Console::printlnR(F("  RGB FADE <r> <g> <b> <ms>   - Fade to RGB color"));
    Console::printlnR(F("  RGB BLINK <interval> [cnt]  - Blink LED"));
    Console::printlnR(F("  RGB STOPBLINK               - Stop blinking"));
    Console::printlnR(F("  RGB TEST                    - Run test sequence"));
    return true;
}

/**
 * Process touch sensor commands
 */
bool CommandListener::processTouchCommands(const String& command) {
    if (!command.startsWith("TOUCH")) {
        return false;
    }
    
    if (!modules || !modules->hasTouchSensor()) {
        Console::printlnR(F("ERROR: Touch sensor not initialized"));
        return true;
    }
    
    // TOUCH STATUS - Show sensor status
    if (command == "TOUCH STATUS") {
        Console::printlnR(modules->getTouchSensor()->getStatus());
        return true;
    }
    
    // TOUCH RESET - Reset statistics
    if (command == "TOUCH RESET") {
        modules->getTouchSensor()->resetStatistics();
        Console::printlnR(F("Touch sensor statistics reset"));
        return true;
    }
    
    // TOUCH DEBOUNCE <ms> - Set debounce delay
    if (command.startsWith("TOUCH DEBOUNCE")) {
        String delayStr = command.substring(15);
        delayStr.trim();
        
        if (delayStr.length() == 0) {
            Console::printlnR(F("Usage: TOUCH DEBOUNCE <milliseconds>"));
            Console::printR(F("Current: "));
            Console::printR(String(modules->getTouchSensor()->getDebounceDelay()));
            Console::printlnR(F("ms"));
            Console::printlnR(F("Recommended: 20-100ms (50ms default)"));
            return true;
        }
        
        unsigned long delay = delayStr.toInt();
        if (delay < 10 || delay > 500) {
            Console::printlnR(F("ERROR: Debounce delay must be 10-500ms"));
            return true;
        }
        
        modules->getTouchSensor()->setDebounceDelay(delay);
        Console::printR(F("Debounce delay set to "));
        Console::printR(String(delay));
        Console::printlnR(F("ms"));
        return true;
    }
    
    // TOUCH LONGPRESS <ms> - Set long press duration
    if (command.startsWith("TOUCH LONGPRESS")) {
        String durationStr = command.substring(16);
        durationStr.trim();
        
        if (durationStr.length() == 0) {
            Console::printlnR(F("Usage: TOUCH LONGPRESS <milliseconds>"));
            Console::printR(F("Current: "));
            Console::printR(String(modules->getTouchSensor()->getLongPressDuration()));
            Console::printlnR(F("ms"));
            Console::printlnR(F("Recommended: 500-3000ms (1000ms default)"));
            return true;
        }
        
        unsigned long duration = durationStr.toInt();
        if (duration < 100 || duration > 10000) {
            Console::printlnR(F("ERROR: Long press duration must be 100-10000ms"));
            return true;
        }
        
        modules->getTouchSensor()->setLongPressDuration(duration);
        Console::printR(F("Long press duration set to "));
        Console::printR(String(duration));
        Console::printlnR(F("ms"));
        return true;
    }
    
    // TOUCH LONGPRESS ENABLE - Enable long press detection
    if (command == "TOUCH LONGPRESS ENABLE") {
        modules->getTouchSensor()->setLongPressEnabled(true);
        Console::printlnR(F("Long press detection enabled"));
        return true;
    }
    
    // TOUCH LONGPRESS DISABLE - Disable long press detection
    if (command == "TOUCH LONGPRESS DISABLE") {
        modules->getTouchSensor()->setLongPressEnabled(false);
        Console::printlnR(F("Long press detection disabled"));
        return true;
    }
    
    // TOUCH TEST - Test touch detection
    if (command == "TOUCH TEST") {
        Console::printlnR(F("Touch Sensor Test Mode"));
        Console::printlnR(F("Touch the sensor to see detection..."));
        Console::printlnR(F("(Type any command to exit test mode)"));
        
        // Simple test loop - will exit on next command
        unsigned long startTime = millis();
        bool lastState = modules->getTouchSensor()->isTouched();
        
        while (millis() - startTime < 10000) {  // 10 second timeout
            modules->getTouchSensor()->update();
            bool currentState = modules->getTouchSensor()->isTouched();
            
            if (currentState != lastState) {
                if (currentState) {
                    Console::printlnR(F("✓ TOUCHED"));
                } else {
                    Console::printR(F("  Released (duration: "));
                    Console::printR(String(modules->getTouchSensor()->getTouchDuration()));
                    Console::printlnR(F("ms)"));
                }
                lastState = currentState;
            }
            
            // Check for serial input to exit
            if (Serial.available() > 0) {
                Console::printlnR(F("Test mode exited"));
                return true;
            }
            
            delay(10);
        }
        
        Console::printlnR(F("Test timeout - returning to normal operation"));
        return true;
    }
    
    // Unknown TOUCH command
    Console::printlnR(F("Unknown TOUCH command. Available:"));
    Console::printlnR(F("  TOUCH STATUS              - Show sensor status"));
    Console::printlnR(F("  TOUCH RESET               - Reset statistics"));
    Console::printlnR(F("  TOUCH DEBOUNCE <ms>       - Set debounce delay"));
    Console::printlnR(F("  TOUCH LONGPRESS <ms>      - Set long press duration"));
    Console::printlnR(F("  TOUCH LONGPRESS ENABLE    - Enable long press"));
    Console::printlnR(F("  TOUCH LONGPRESS DISABLE   - Disable long press"));
    Console::printlnR(F("  TOUCH TEST                - Test touch detection"));
    return true;
}
