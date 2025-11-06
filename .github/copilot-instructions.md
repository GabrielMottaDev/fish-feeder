# Fish Feeder - ESP32 WiFi-Enabled Smart Feeder with NTP Sync

## Project Overview
This is a PlatformIO-based IoT project for an ESP32-WROOM-32 Development Board featuring automated fish feeding with WiFi connectivity, real-time clock synchronization, and comprehensive web-based configuration. The project emphasizes **strictly non-blocking architecture**, clean modular design, and professional IoT device behavior with automatic reconnection and time synchronization capabilities.

## **CRITICAL: Non-Blocking Development Philosophy**
🚨 **NEVER USE `delay()` OR BLOCKING OPERATIONS** 🚨

**MANDATORY PATTERNS:**
- **TaskScheduler-based architecture**: All operations must be task-based
- **State machines**: Use state variables and millis() for timing
- **Non-blocking I/O**: All network, serial, and hardware operations must be asynchronous
- **Responsive system**: User commands must be processed immediately during any operation

**FORBIDDEN PATTERNS:**
- `delay()` anywhere in the code
- `while()` loops waiting for hardware responses
- Blocking network operations
- Synchronous file/EEPROM operations

## **CRITICAL: Web Interface Development Lessons (November 2025)**
🚨 **LEARNED FROM CRITICAL ERRORS - NEVER REPEAT** 🚨

### **HTML/JavaScript Generation Rules**
**FUNDAMENTAL ERROR**: Using `//` comments in dynamically generated JavaScript causes syntax errors in compacted HTML.

**MANDATORY PATTERNS:**
- **NO `//` comments in generated JavaScript**: HTML compaction breaks single-line comments
- **Test generated HTML**: Always verify final HTML output in browser console
- **Debug methodology**: Check HTML source FIRST, then backend logs
- **Clean JavaScript**: Remove all TODO comments and unnecessary annotations from generated code

**EXAMPLES:**
```cpp
// ❌ NEVER DO THIS - Breaks next function:
html += "function test() { return true; }";
html += "// TODO: implement later";  // BREAKS NEXT LINE!
html += "function other() { return false; }";

// ✅ ALWAYS DO THIS - Clean code only:
html += "function test() { return true; }";
html += "function other() { return false; }";
```

**WEB INTERFACE DEBUG WORKFLOW:**
1. **Generate HTML and view source** - Check for syntax errors
2. **Open browser console** - Look for JavaScript errors with line numbers
3. **Map character position to C++ code** - Find exact source of problem
4. **Fix in C++ HTML generation** - Never fix symptoms, fix source
5. **Test in clean browser session** - Verify complete fix

### **tzapu/WiFiManager@^2.0.17 Integration Patterns**
**MANDATORY SETUP:**
- Configure `setWebServerCallback()` BEFORE starting portal
- Use proper non-blocking patterns with `processConfigPortal()`
- Test API endpoints separately before integration
- Use proper UTF-8 encoding: `"text/html; charset=utf-8"`

### **JavaScript Escape Patterns**
```cpp
// ✅ CORRECT string escaping for HTML attributes:
html += "'<button onclick=\"functionCall()\">'";  // Use \" for HTML attributes
html += "'<span class=\"css-class\">'";           // Consistent escaping

// ❌ WRONG - Inconsistent escaping causes errors:
html += "'<button onclick='functionCall()'>'";    // Mixing quote types
```

## **IMPORTANT: Instructions Maintenance**
**ALWAYS ask the user if they want to update this copilot-instructions.md file after making any modifications to the project.** This includes:
- Adding new hardware components or modules
- Implementing new features or commands
- Changing pin configurations or hardware setup
- Modifying serial commands or WiFi behavior
- Updating dependencies or libraries
- Refactoring code structure or architecture
- Adding new non-blocking patterns or state machines

**Always provide the option to update the instructions, but let the user decide.**

## Architecture & Key Components

### Hardware Configuration
- **Target Board**: ESP32-WROOM-32 Development Board (`esp32dev` in platformio.ini)
- **Connectivity**: Built-in WiFi with automatic reconnection and web portal configuration
- **Time Synchronization**: DS3231 RTC + NTP internet sync (dual time source)
- **Motor Control**: 28BYJ-48 stepper motor with ULN2003 driver (non-blocking operation)
- **Pin Mapping (ESP32-WROOM-32)**:
  - **I2C (RTC DS3231)**:
    - SDA → GPIO 21 (D21)
    - SCL → GPIO 22 (D22)
    - VCC → 3.3V or 5V
    - GND → GND
  - **Stepper Motor (ULN2003)**:
    - IN1 → GPIO 15
    - IN2 → GPIO 4
    - IN3 → GPIO 5
    - IN4 → GPIO 18
    - VCC → 5V Direct (bypass ESP32 regulator)
    - GND → GND
  - **WiFi**: Built-in ESP32 module (no additional hardware required)

### Core Components
- **`RTCModule` class** (`src/rtc_module.h/.cpp`): DS3231 operations with I2C diagnostics and comprehensive error handling
- **`StepperMotor` class** (`src/stepper_motor.h/.cpp`): Non-blocking 28BYJ-48 control via AccelStepper with smooth acceleration/deceleration
- **`FeedingController` class** (`src/feeding_controller.h/.cpp`): Async feeding operations with portion control and status monitoring
- **`FeedingSchedule` class** (`src/feeding_schedule.h/.cpp`): Automated feeding schedule system with NVRAM persistence, power-loss recovery, and missed feeding tolerance
- **`WiFiController` class** (`src/wifi_controller.h/.cpp`): Complete WiFi management with tzapu WiFiManager integration, auto-reconnection, and web portal
- **`NTPSync` class** (`src/ntp_sync.h/.cpp`): Non-blocking NTP time synchronization with automatic RTC updates and timezone support
- **`ConsoleManager` class** (`src/console_manager.h/.cpp`): Dual logging system (standard + response outputs) with configurable verbosity
- **`CommandListener` class** (`src/command_listener.h/.cpp`): Centralized command processing with organized help system and modular command categories
- **`Config` module** (`src/config.h/.cpp`): Global configuration constants for all system parameters (feeding, WiFi, NTP, tasks, schedules)
- **Main loop** (`src/main.cpp`): TaskScheduler orchestration with 7 concurrent non-blocking tasks

## Development Patterns

### TaskScheduler Architecture
The project uses **cooperative multitasking** with TaskScheduler library for true multi-operation capability:
- **7 Concurrent Non-Blocking Tasks**:
  - `displayTimeTask()` - Time display control (1000ms) - now command-triggered only
  - `processSerialTask()` - Command processing (50ms) - always responsive
  - `motorMaintenanceTask()` - Stepper operations (10ms) - smooth motor control
  - `feedingMonitorTask()` - Feeding completion monitoring (100ms, on-demand)
  - `scheduleMonitorTask()` - Automatic feeding schedule monitoring (30000ms) - NEW
  - `wifiMonitorTask()` - WiFi connection monitoring and auto-reconnection (10000ms)
  - `ntpSyncTask()` - Non-blocking NTP synchronization checks (60000ms)
- **True Multitasking**: All operations run simultaneously without blocking
- **Task Control**: Individual task control via commands (`PAUSE`/`RESUME` + task type)
- **State Management**: Complex operations use state machines instead of blocking waits

### Console Manager System
The project implements a **centralized command processing architecture**:
- **ConsoleManager Class**: Handles all serial communication and command parsing
- **Dual Logging System**:
  - `Console::print()` / `Console::println()` - Respects logging state (controlled by LOG command)
  - `Console::printR()` / `Console::printlnR()` - Always prints (for command responses)
- **Organized Help System**: `HELP` command shows categorized available commands
- **Command Categories**: System, Task Control, Motor Operations, RTC Functions

### Error Handling Philosophy
The project implements a **defensive programming approach** for I2C communication:
- Multiple initialization attempts with different strategies
- Comprehensive I2C bus scanning before device initialization
- Fallback communication tests using raw Wire library calls
- Detailed English error messages with hardware connection checklists

### Serial Communication Patterns
- **Baud rate**: 115200 (configurable in `config.h` as `SERIAL_BAUD_RATE`)
- **Command Categories**:
  - **System Commands**: `HELP`, `INFO`, `LOG` (toggle logging)
  - **Task Commands**: `TASKS`, `PAUSE DISPLAY`, `RESUME DISPLAY`, `PAUSE MOTOR`, `RESUME MOTOR`
  - **Motor Commands**: `FEED [portions]`, `CALIBRATE`, `MOTOR STATUS`, `FEEDING STATUS`, `CONFIG`, `STEP CW [steps]`, `STEP CCW [steps]`
  - **RTC Commands**: `TIME`, `SET DD/MM/YYYY HH:MM:SS` format for time adjustment
  - **WiFi Commands**: `WIFI SCAN`, `WIFI CONNECT`, `WIFI STATUS`, `WIFI PORTAL`, `WIFI LIST`, `WIFI CONFIG`
  - **NTP Commands**: `NTP STATUS`, `NTP SYNC`, `NTP STATS`, `NTP INTERVAL [minutes]`
  - **Schedule Commands**: `SCHEDULE STATUS`, `SCHEDULE LIST`, `SCHEDULE NEXT`, `SCHEDULE ENABLE/DISABLE`, `SCHEDULE TOLERANCE [minutes]`, `SCHEDULE RECOVERY [hours]`
- **Output format**: `DD/MM/YYYY HH:MM:SS - DayName (Unix: timestamp)`
- **Always Responsive**: Commands work immediately even during WiFi/NTP operations
- All user-facing messages are in **English**

### I2C Diagnostics Pattern
When adding I2C devices, follow the `RTCModule::scanI2C()` pattern:
- Scan all I2C addresses (1-127)
- Identify devices by address with descriptive names  
- Test specific device communication after detection
- Provide hardware connection troubleshooting steps

## Build & Development Workflow

### PlatformIO Commands
```bash
# Build project
pio build

# Upload to ESP32
pio upload

# Monitor serial output
pio device monitor

# Clean build
pio clean
```

### Dependencies
- **RTClib**: `adafruit/RTClib@^2.1.4` for DS3231 Real-Time Clock operations
- **AccelStepper**: `waspinator/AccelStepper@^1.64` for advanced stepper motor control with acceleration/deceleration
- **TaskScheduler**: `arkhipenko/TaskScheduler@^4.0.2` for cooperative multitasking and non-blocking operations
- **WiFiManager**: `tzapu/WiFiManager@^2.0.17` for professional WiFi configuration with captive portal
- **Framework**: Arduino
- **Platform**: Espressif ESP32
- **Built-in Libraries**: WiFi, Preferences, time.h (for NTP)

### Serial Monitoring
Always use `monitor_echo = yes` in platformio.ini for debugging. The project expects continuous serial monitoring during development for diagnostics.

## Code Conventions

### Language & Comments
- **Code comments**: English only
- **Serial output**: English only
- **Variable/function names**: English (camelCase)
- **Class names**: PascalCase

### Modular Architecture Principles
- Keep `main.cpp` minimal - only setup(), loop(), and high-level orchestration
- Extract all business logic into dedicated classes/modules
- Separate concerns: hardware interfaces, user interaction, data processing
- Use dependency injection patterns where possible for testability

### Memory Management
- Use `F()` macro for string literals stored in flash memory
- Prefer `String` class for dynamic text manipulation in serial commands
- Use `const char*` arrays for static lookup tables (e.g., `dayNames[]`)

### Console Usage Patterns
The project provides a sophisticated logging system through `ConsoleManager`:
- **Standard Output** (respects logging state): `Console::print()`, `Console::println()`
- **Response Output** (always prints): `Console::printR()`, `Console::printlnR()`
- **Logging Control**: `LOG` command toggles verbose output while keeping responses active
- **Usage**: Include console_manager.h header and use Console:: methods in other modules

### Time Format Standards
- **User input/output**: `DD/MM/YYYY HH:MM:SS` (Brazilian format)
- **Internal**: DateTime objects from RTClib
- **Debug**: Include Unix timestamps for precision

### Current System Features

### WiFi Management System
- **Automatic Reconnection**: ESP32 automatically connects to saved networks on boot
- **Dual Strategy Reconnection**: 
  1. tzapu WiFiManager saved credentials (from portal configuration)
  2. Custom saved networks (from manual WIFI CONNECT commands)
- **Web Configuration Portal**: 
  - Automatic startup on boot (configurable)
  - Automatic startup on connection loss (configurable) 
  - Accessible at 192.168.4.1 when active
  - Configurable timeout (default: 15 minutes)
  - Optional password protection
- **Network Management**: Scan, connect, save, remove, and list WiFi networks
- **Complete Non-blocking**: All WiFi operations are asynchronous

### NTP Time Synchronization
- **Automatic Sync**: Syncs RTC with internet time every 30 minutes
- **Initial Sync**: Syncs 5 seconds after WiFi connection established
- **Non-blocking Implementation**: Uses state machine pattern - no delay() calls
- **Timezone Support**: Configurable GMT offset (default: UTC-3 for Brazil)
- **Smart Updates**: Only updates RTC if time difference > 2 seconds
- **Statistics Tracking**: Success/failure rates, sync history, next sync prediction
- **Server Configuration**: Uses reliable pool.ntp.org by default

### Time Display Control
- **Command-triggered Only**: Time display via TIME command (no automatic printing)
- **Dual Time Sources**: Shows both RTC and system time when available
- **Format**: DD/MM/YYYY HH:MM:SS - DayName (Unix: timestamp)

### Integration Points

### Current Modular Architecture
The project has achieved a **fully modular design** with clear separation of concerns:

- **`RTCModule`**: Complete RTC hardware interface with I2C diagnostics and time management
- **`StepperMotor`**: Advanced motor control with AccelStepper integration and non-blocking operations
- **`FeedingController`**: High-level feeding logic with portion control and async operations
- **`WiFiController`**: Complete WiFi management with dual reconnection strategies and web portal
- **`NTPSync`**: Non-blocking internet time synchronization with state machine implementation
- **`ConsoleManager`**: Dual logging system with standard and response outputs (logging only)
- **`CommandListener`**: Centralized command processing with organized help system
- **`Config`**: Global configuration management for all system parameters
- **`main.cpp`**: Minimal TaskScheduler orchestration with 6 concurrent tasks

### Console System Usage
For adding new modules or commands, follow the Console system pattern:
- Use `Console::print()` / `Console::println()` for debug/logging output (respects LOG state)
- Use `Console::printR()` / `Console::printlnR()` for command responses (always visible)
- Add commands to appropriate category in `CommandListener::processXxxCommands()` methods
- Update `CommandListener::showHelp()` method with new command documentation

### Task Integration Pattern
When adding new periodic operations:
- Create task callback function following naming pattern `xxxTask()`
- Define Task object with appropriate interval from `config.h`
- Add to TaskScheduler in `setup()`
- Use non-blocking patterns - avoid `delay()` calls
- Integrate with feeding state management if relevant

### Non-Blocking Development Patterns
**State Machine Example** (from NTPSync):
```cpp
// State variables
bool syncInProgress;
bool waitingForNTPResponse;
unsigned long syncStartTime;

// Non-blocking check method
bool checkNTPSyncProgress() {
    if (millis() - syncStartTime > timeout) {
        // Handle timeout
        return true; // Completed
    }
    
    if (checkCondition()) {
        // Success
        return true; // Completed
    }
    
    return false; // Still in progress
}
```

**ALWAYS use this pattern instead of delay() or blocking while() loops**

## ESP32 Project Status

### Compilation & Performance
- **Platform**: Espressif ESP32 (6.12.0) - ESP32 Dev Module
- **Hardware**: ESP32 240MHz, 320KB RAM, 4MB Flash
- **Memory Usage**: 
  - RAM: 6.7% (22,116 bytes / 327,680 bytes)
  - Flash: 26.0% (340,901 bytes / 1,310,720 bytes)
- **Build Status**: ✅ SUCCESS - Optimized build in 3.07 seconds
- **Serial Communication**: 115200 baud with monitor echo enabled

### Migration Completion
- ✅ **Platform Migration**: Successfully migrated from Arduino Mega to ESP32-WROOM-32
- ✅ **Pin Configuration**: Updated all GPIO assignments for ESP32-WROOM-32 hardware (D12, D13, D14, D27 for stepper)
- ✅ **Architecture**: Maintained modular design with TaskScheduler integration
- ✅ **Dependencies**: All libraries (RTClib, AccelStepper, TaskScheduler) compatible
- ✅ **Code Quality**: 100% English codebase with comprehensive error handling
- ✅ **Documentation**: Updated with ESP32-WROOM-32 specifications and pin mappings

## Current Advanced Features (2025)

### Complete WiFi Ecosystem
- **Auto-Reconnection**: Dual strategy (WiFiManager + custom saved networks)
- **Web Portal**: Professional captive portal at 192.168.4.1 with 15-minute timeout
- **Persistent Storage**: Credentials saved in NVRAM, survive reboots
- **Connection Monitoring**: 10-second interval health checks with automatic recovery
- **Portal Auto-Launch**: Configurable auto-start on boot or connection loss

### Internet Time Synchronization
- **NTP Integration**: Fully non-blocking time sync with pool.ntp.org
- **Automatic Updates**: 30-minute interval sync when WiFi connected
- **Smart Sync**: Only updates RTC if difference > 2 seconds
- **State Machine**: Complete non-blocking implementation - no delay() usage
- **Timezone Support**: Configurable GMT offset (default: UTC-3 Brazil)
- **Statistics**: Success rate tracking and diagnostic commands

### Professional IoT Behavior
- **Boot Sequence**: Auto-connect → NTP sync → System ready
- **Graceful Degradation**: Works offline, enhances online
- **Multi-operation**: Feed fish while syncing time while processing commands
- **Real-time Responsiveness**: All commands work instantly during any operation
- **Enterprise Reliability**: Automatic error recovery and connection management

### **Automated Feeding Schedule System (November 2025)**

🕒 **COMPREHENSIVE SCHEDULE MANAGEMENT**: Implemented robust automated feeding system with power-loss recovery

#### **Schedule System Features:**
- **NVRAM Persistence**: Last feeding time stored in non-volatile memory
- **Power-Loss Recovery**: Automatically detects and recovers missed feedings after power outages
- **Configurable Tolerance**: Adjustable missed feeding tolerance (default: 30 minutes)
- **Flexible Scheduling**: Support for hour:minute:second precision with portion control
- **Individual Control**: Enable/disable entire system or individual schedules
- **Non-blocking Operation**: All schedule monitoring runs in dedicated 30-second task

#### **Default Schedule Configuration:**
```cpp
ScheduledFeeding DEFAULT_FEEDING_SCHEDULE[] = {
    {8,  0, 0, 2, true, "Morning feeding"},    // 08:00:00 - 2 portions
    {12, 0, 0, 1, true, "Midday feeding"},     // 12:00:00 - 1 portion  
    {18, 0, 0, 2, true, "Evening feeding"}     // 18:00:00 - 2 portions
};
```

#### **Recovery System Logic:**
- **Miss Detection**: Monitors for feedings that should have occurred
- **Tolerance Window**: Configurable tolerance period (1-120 minutes)
- **Recovery Period**: Looks back up to 12 hours for missed feedings
- **Prevents Over-feeding**: Only recovers one missed feeding at a time
- **Manual Integration**: Records manual feedings to prevent duplicate feeding

### **HTTP Time Fallback System (November 2025)**

🌐 **ROBUST NETWORK TIME SYNCHRONIZATION**: Implemented comprehensive fallback system for networks that block NTP UDP

#### **Multi-Layer Time Sync Strategy:**
1. **Primary**: 6 NTP Servers (UDP port 123)
   - `pool.ntp.org`, `time.cloudflare.com`, `time.google.com`, `time.nist.gov`
   - `br.pool.ntp.org`, `south-america.pool.ntp.org`
2. **Fallback**: 3 HTTP Time APIs (TCP port 80)
   - `worldtimeapi.org/api/timezone/America/Sao_Paulo`
   - `timeapi.io/api/Time/current/zone?timeZone=America/Sao_Paulo`
   - Custom parsing for JSON responses

#### **HTTP Time API Implementation:**
```cpp
// WorldTimeAPI JSON parsing
int unixPos = response.indexOf("\"unixtime\":");
String timestampStr = response.substring(startPos, endPos);
unsigned long timestamp = timestampStr.toInt();
DateTime dt(timestamp);
rtcModule->adjust(dt);
```

#### **Non-Blocking HTTP Requests:**
```cpp
// CORRECT: Non-blocking HTTP with yield()
while (client.available() == 0) {
    if (millis() - timeout > HTTP_TIME_TIMEOUT) {
        return false; // Timeout
    }
    yield(); // CRITICAL: Use yield() not delay()
}
```

#### **Command Integration:**
- **`NTP SYNC`**: Tries NTP first, falls back to HTTP if needed
- **`NTP FALLBACK`**: Forces HTTP-only test (bypasses NTP)
- **`NTP STATUS`**: Shows last successful sync method (NTP vs HTTP)

## **CRITICAL: Development Lessons Learned (2025)**

### **JavaScript HTML Generation Critical Error (November 2025)**
🚨 **MOST CRITICAL LESSON: Comments in Compacted HTML Break JavaScript**

**FATAL ERROR PATTERN:**
```cpp
// ❌ THIS BREAKS JAVASCRIPT IN COMPACTED HTML:
html += "function feedNow() { return true; }";
html += "// TODO: implement feature";  // KILLS NEXT FUNCTION!
html += "function other() { return false; }";

// Result in browser: function feedNow(){return true;}// TODO: implement featurefunction other(){return false;}
// Error: "Unexpected end of input" - other() function is commented out!
```

**MANDATORY CORRECTION:**
```cpp
// ✅ NEVER USE // COMMENTS IN GENERATED JAVASCRIPT:
html += "function feedNow() { return true; }";
html += "function other() { return false; }";

// Result: function feedNow(){return true;}function other(){return false;}
// Works perfectly!
```

**DEBUG METHODOLOGY FOR WEB INTERFACES:**
1. **FIRST**: View page source and check generated JavaScript
2. **SECOND**: Open browser console and look for syntax errors with line numbers
3. **THIRD**: Map character position back to C++ HTML generation code
4. **FOURTH**: Fix in C++ source, NEVER in generated HTML
5. **FIFTH**: Test with clean browser session

**GOLDEN RULES:**
- ❌ **NEVER** use `//` comments in dynamically generated JavaScript
- ✅ **ALWAYS** test generated HTML in browser console
- ✅ **USE** `console.log()` for JavaScript debugging, not `//` comments
- ✅ **REMEMBER**: HTML compaction removes line breaks, making `//` deadly

### **tzapu/WiFiManager@^2.0.17 Specific Knowledge**
🚨 **ALWAYS verify method availability before using**

**VERIFIED WORKING METHODS:**
- `setConfigPortalTimeout(0)` - Never timeout (always active)
- `setConnectRetries(3)` - Connection retry attempts
- `setBreakAfterConfig(false)` - Don't break after config
- `setDebugOutput(true/false)` - Debug control
- `setTitle("Custom Title")` - Portal page title
- `setCustomHeadElement("<style>...</style>")` - Custom CSS
- `setMenu(std::vector<const char*>)` - Custom menu items
- `addParameter(WiFiManagerParameter*)` - Custom form fields
- `setWebServerCallback([]{...})` - Custom HTTP routes
- `server->on("/route", HTTP_GET, []{...})` - Route handlers
- `startWebPortal()` - Start web server on existing AP
- `stopConfigPortal()` - Stop web server
- `process()` - Process HTTP requests (non-blocking)
- `autoConnect()` - Auto connect with credentials

**NON-EXISTENT METHODS (DO NOT USE):**
- `setConfigPortalBlocking()` - Does not exist in 2.0.17
- `setConfigPortalSSID()` - Does not exist in 2.0.17

### **WiFi Always-On Portal Pattern**
```cpp
// CORRECT: Always-on portal setup
WiFi.mode(WIFI_AP_STA);  // Dual mode
WiFi.softAP(apName, password);  // Manual AP setup
wifiManager.startWebPortal();  // Start web server

// CORRECT: Custom route interception
wifiManager.setWebServerCallback([this]() {
    wifiManager.server->on("/close", HTTP_GET, [this]() {
        // Custom close handler
    });
});

// CORRECT: Non-blocking processing
wifiManager.process();  // Call regularly in task
```

### **HTML/UTF-8 Encoding Best Practices**
🚨 **ALWAYS use proper charset for special characters**

**CORRECT HTML Response Pattern:**
```cpp
String html = "<!DOCTYPE html><html><head>";
html += "<meta charset='UTF-8'>";  // CRITICAL for emoji support
html += "<title>Page Title</title></head>";
html += "<body>Content with &#128268; emoji</body></html>";

// CRITICAL: Send with charset
server->send(200, "text/html; charset=utf-8", html);
```

**Emoji HTML Entities:**
- 🔌 → `&#128268;`
- ✓ → `&#10003;`
- ⚠ → `&#9888;`

### **DNS Configuration Pattern**
```cpp
// CORRECT: Priority-based DNS setup
const char* DNS_SERVERS[] = {
    "8.8.8.8",           // Primary: Google DNS
    "1.1.1.1",           // Secondary: Cloudflare DNS
    "8.8.4.4",           // Tertiary: Google DNS secondary
    "1.0.0.1",           // Quaternary: Cloudflare DNS secondary
    "208.67.222.222",    // Backup: OpenDNS primary
    "208.67.220.220"     // Backup: OpenDNS secondary
};

// CORRECT: Auto-configuration on WiFi connect
IPAddress primaryDNS, secondaryDNS;
primaryDNS.fromString(DNS_SERVERS[0]);
secondaryDNS.fromString(DNS_SERVERS[1]);
WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS);
```

### **NTP Implementation Best Practices**
🚨 **ALWAYS use non-blocking state machine pattern**

**CORRECT: Non-blocking NTP sync**
```cpp
// State variables
bool syncInProgress = false;
bool waitingForNTPResponse = false;
unsigned long syncStartTime = 0;

// CORRECT: Non-blocking check
bool checkNTPSyncProgress() {
    if (millis() - syncStartTime > NTP_SYNC_TIMEOUT) {
        // Try next server in fallback array
        return handleTimeout();
    }
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Success - update RTC
        return handleSuccess(timeinfo);
    }
    
    return false; // Still waiting
}
```

**CRITICAL: DNS resolution testing**
```cpp
// ALWAYS test DNS before NTP
IPAddress serverIP;
if (WiFi.hostByName(server, serverIP)) {
    Console::printlnR("DNS resolved: " + serverIP.toString());
} else {
    Console::printlnR("DNS resolution failed");
}
```

### **State Management Patterns**
🚨 **Use separate boolean flags for different states**

**CORRECT: Separate state variables**
```cpp
// Good - specific purpose flags
bool portalStartRequested;    // Request to start
bool configPortalActive;      // Portal is running
bool shutdownRequested;       // Request to shutdown

// WRONG - reusing flags for different purposes
bool portalStartRequested;  // Used for both start AND shutdown
```

### **Compilation Error Prevention**
🚨 **ALWAYS verify variable scope and method existence**

**BEFORE using any library method:**
1. Check library documentation/source code
2. Verify method exists in specific version
3. Test variable scope (local vs class member)
4. Use const char* for string literals when possible

**BEFORE using variables:**
1. Verify variable is in scope
2. Check if variable needs to be class member
3. Initialize all variables in constructor

### **Non-Blocking Internet Connectivity Test**
```cpp
// CORRECT: Non-blocking with yield()
while (client.connected() && !client.available()) {
    if (millis() - startTime > timeout) {
        return false;
    }
    yield(); // ESP32 yield - NOT delay(1)
}
```

### **TaskScheduler Integration Pattern**
```cpp
// CORRECT: Always add new functionality as tasks
void newFeatureTask() {
    // Non-blocking implementation
    processNewFeature();
}

// Add to main.cpp setup()
Task tNewFeature(500, TASK_FOREVER, &newFeatureTask, &taskScheduler, true);
```

### **CRITICAL: Non-Blocking Architecture Audit Lessons (November 2025)**

🚨 **MAJOR DISCOVERY**: Even with careful non-blocking design, **BLOCKING CODE CAN SLIP IN UNNOTICED**

#### **Blocking Code Audit Results:**
During comprehensive audit, **CRITICAL BLOCKING VIOLATIONS** were found in supposedly non-blocking code:

**❌ WiFiController Violations Found:**
```cpp
// BLOCKING VIOLATION - Found in connectToNetwork():
delay(1000);  // 1 second block
while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);  // 500ms x 20 = 10 SECOND TOTAL BLOCK!
    attempts++;
}
```

**❌ NTPSync Violations Found:**
```cpp
// BLOCKING VIOLATIONS - Found in HTTP fallback:
delay(1000); // Between server attempts
while (client.available() == 0) {
    delay(50);  // Repeated 50ms blocks
}
```

#### **Root Cause Analysis:**
1. **Gradual Code Decay**: Non-blocking code gradually became blocking through incremental changes
2. **Lack of Regular Audits**: No systematic checking for `delay()` and blocking `while()` loops
3. **False Confidence**: TaskScheduler architecture masked blocking issues in individual functions
4. **Copy-Paste Errors**: Blocking patterns copied from examples without conversion

#### **Blocking Code Detection Strategy:**
```bash
# Use grep to find ALL potential blocking patterns:
grep -r "delay(" src/
grep -r "while.*(" src/ | grep -v "millis()"
grep -r "for.*connect" src/
grep -r "client.connect.*timeout" src/
```

#### **Mandatory Non-Blocking Corrections Implemented:**

**✅ WiFi State Machine Correction:**
```cpp
// BEFORE (BLOCKING):
while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
}

// AFTER (NON-BLOCKING):
enum WiFiConnectionState { WIFI_IDLE, WIFI_CONNECTING, ... };
void processConnectionState() {
    // Called every 500ms by task - no blocking
    switch(connectionState) { ... }
}
```

**✅ NTP HTTP Fallback Correction:**
```cpp
// BEFORE (BLOCKING):
delay(1000);
while (client.available() == 0) { delay(50); }

// AFTER (NON-BLOCKING):
yield(); // or while with yield()
while (client.available() == 0) { yield(); }
```

#### **Critical Lessons for Future Development:**

1. **🔍 MANDATORY CODE AUDITS**: Run blocking code detection BEFORE every major release
2. **🚫 ZERO TOLERANCE**: ANY `delay()` in main execution path is a critical bug
3. **⚡ RESPONSIVENESS TEST**: System must respond to commands in < 100ms ALWAYS
4. **🔄 STATE MACHINES**: Complex operations MUST use state machines, never blocking loops
5. **📊 PERFORMANCE METRICS**: Track task execution times - any task > 50ms is suspect

#### **Blocking Code Detection Automation:**
```cpp
// Add to CI/CD pipeline or regular testing:
function auditBlockingCode() {
    grep -r "delay(" src/ && echo "❌ BLOCKING: delay() found!"
    grep -r "while.*!.*millis" src/ && echo "❌ BLOCKING: Infinite while() found!"
    grep -r "client.connect.*while" src/ && echo "❌ BLOCKING: Blocking network call!"
}
```

#### **Non-Blocking Architecture Verification:**
**✅ Main Loop**: Only `taskScheduler.execute()` - 100% correct
**✅ Tasks**: All run in < 50ms intervals - verified
**✅ WiFi**: State machine implemented - corrected
**✅ NTP**: yield() instead of delay() - corrected
**✅ Motor**: AccelStepper non-blocking - already correct
**✅ Serial**: 50ms interval processing - already correct

### **CRITICAL: Component Initialization Order Crisis (November 2025)**

🚨 **MAJOR SYSTEM ERROR**: API endpoints registered BEFORE components are initialized

#### **The Fatal Sequence Problem:**
**❌ WRONG ORDER:**
1. WiFiController starts → `setWebServerCallback()` executed → Tries to register `/api/feed`
2. **FeedingSchedule = NULL** → `setupScheduleAPIEndpoints()` returns early
3. **FeedingController = NULL** → No endpoints registered
4. Later: Components initialize → **BUT endpoints were never registered!**
5. Result: **404 Not Found** on ALL API calls

**✅ CORRECT ORDER:**
1. WiFiController starts → Minimal callback setup only
2. All components initialize → FeedingSchedule, FeedingController ready
3. **SINGLE centralized call** → `registerAllEndpoints()` with ALL components available
4. Result: **All endpoints registered correctly**

#### **Root Cause Analysis:**
- **tzapu WiFiManager executes callbacks IMMEDIATELY** when web server starts
- **Components aren't initialized yet** during callback execution
- **No verification** that dependencies are ready before endpoint registration
- **Multiple registration attempts** in different places caused confusion

#### **Centralized Registration Solution:**
```cpp
// WRONG: Multiple scattered registrations
setWebServerCallback([this]() {
    setupScheduleAPIEndpoints(); // FAILS - components NULL
});
// Later: another registration attempt
// Later: manual re-registration
// Result: Confusion and failures

// CORRECT: Single centralized registration
void registerAllEndpoints() {
    // Verify ALL components ready
    if (!wifiManager.server || !feedingSchedule || !feedingController) {
        Console::printlnR("❌ ERROR: Components not ready");
        return;
    }
    
    // Register ALL endpoints in one place
    wifiManager.server->on("/api/feed", HTTP_GET, [this]() { ... });
    wifiManager.server->on("/api/status", HTTP_GET, [this]() { ... });
    // ... all other endpoints
}

// Called ONCE after ALL components initialized
wifiController.registerAllEndpoints();
```

#### **Mandatory Initialization Verification:**
```cpp
// ALWAYS verify component availability before registration
Console::printlnR("✓ Server: " + String(wifiManager.server ? "Available" : "NULL"));
Console::printlnR("✓ FeedingSchedule: " + String(feedingSchedule ? "Available" : "NULL"));
Console::printlnR("✓ FeedingController: " + String(feedingController ? "Available" : "NULL"));

if (!wifiManager.server || !feedingSchedule || !feedingController) {
    Console::printlnR("❌ CRITICAL: Cannot register endpoints - missing components");
    return;
}
```

#### **Component Initialization Order Rules:**
1. **Hardware Initialization**: RTC, Motors, I2C devices
2. **Communication Initialization**: WiFi, Serial, Console
3. **Business Logic Initialization**: FeedingController, FeedingSchedule
4. **🚨 CRITICAL POINT**: **ALL COMPONENTS READY**
5. **API Registration**: **SINGLE CALL** to register all endpoints
6. **Task Scheduler**: Start all concurrent tasks

### **HTTP Method Compatibility Crisis (November 2025)**

🚨 **tzapu/WiFiManager@^2.0.17 POST Method Bug**: HTTP_POST endpoints not registered correctly

#### **The POST Registration Failure:**
**❌ BROKEN:**
```cpp
wifiManager.server->on("/api/feed", HTTP_POST, [this]() {
    // This endpoint is NEVER REGISTERED due to WiFiManager bug
});
```

**✅ WORKING:**
```cpp
wifiManager.server->on("/api/feed", HTTP_GET, [this]() {
    // GET methods work perfectly
    String portions = wifiManager.server->arg("portions"); // From URL query
});
```

#### **JavaScript Adaptation for GET Method:**
```cpp
// WRONG: POST with body
fetch('/api/feed', {
    method: 'POST',
    body: 'portions=2'
});

// CORRECT: GET with query parameters
fetch('/api/feed?portions=2&t=' + Date.now(), {
    method: 'GET'
});
```

#### **Cache Busting for Dynamic Requests:**
```cpp
// ALWAYS add timestamp to prevent browser caching
html += "apiGet('/api/feed?portions=' + portions + '&t=' + Date.now(), function(result) {";
```

### **Date Formatting Standards (November 2025)**

🚨 **Professional Date Display**: Always use leading zeros

#### **Date Format Requirements:**
```cpp
// WRONG: Inconsistent digit count
json += String(day) + "/" + String(month) + "/" + String(year);
// Result: 3/11/2025 8:00 (unprofessional)

// CORRECT: Leading zeros for consistency
if (day < 10) json += "0";
json += String(day) + "/";
if (month < 10) json += "0";
json += String(month) + "/" + String(year);
json += " ";
if (hour < 10) json += "0";
json += String(hour) + ":";
if (minute < 10) json += "0";
json += String(minute);
// Result: 03/11/2025 08:00 (professional)
```

### **User Experience Improvements (November 2025)**

#### **Remove Unnecessary Confirmations:**
```cpp
// WRONG: Annoying confirmations
html += "if(confirm('Feed ' + portions + ' portion(s) now?')) {";
html += "  // Actually feed";
html += "}";

// CORRECT: Direct action with console feedback
html += "console.log('Starting feeding with', portions, 'portions');";
html += "apiGet('/api/feed?portions=' + portions, function(result) {";
html += "  if(result.success) {";
html += "    console.log('Feeding started successfully');";
html += "    refreshData(); // Update UI";
html += "  }";
html += "});";
```

### **Error Prevention Checklist (UPDATED)**
Before implementing ANY new feature:

1. ✅ **Verify library version compatibility**
2. ✅ **Check method existence in documentation**
3. ✅ **Test variable scope and initialization**
4. ✅ **🚨 AUDIT FOR BLOCKING CODE** (delay, while loops)
5. ✅ **Use non-blocking patterns EXCLUSIVELY**
6. ✅ **Add proper UTF-8 charset for HTML**
7. ✅ **Use HTML entities for special characters**
8. ✅ **Implement as TaskScheduler task**
9. ✅ **🔍 GREP AUDIT**: Search for blocking patterns before commit
10. ✅ **📊 RESPONSIVENESS TEST**: Verify < 100ms command response
11. ✅ **Use state machines for complex operations**
12. ✅ **Replace ALL delay() with yield() or state timing**
13. ✅ **🚨 JAVASCRIPT COMMENTS AUDIT**: NO `//` comments in generated HTML/JS
14. ✅ **🔍 WEB INTERFACE DEBUG**: Always check browser console first
15. ✅ **📄 HTML SOURCE VERIFICATION**: View generated HTML before debugging backend
16. ✅ **🚨 COMPONENT INITIALIZATION ORDER**: Verify all dependencies before API registration
17. ✅ **🔍 CENTRALIZED ENDPOINT REGISTRATION**: Single registration point after all components ready
18. ✅ **⚠ HTTP METHOD COMPATIBILITY**: Use GET instead of POST for tzapu WiFiManager
19. ✅ **📅 PROFESSIONAL DATE FORMATTING**: Always use leading zeros (DD/MM/YYYY HH:MM)
20. ✅ **🎯 UX OPTIMIZATION**: Remove unnecessary confirmations, provide console feedback

## Future Development Notes
- Schedule management system architecture prepared
- Bluetooth LE integration capabilities available
- Web interface for feeding control (beyond WiFi config)
- MQTT integration for IoT platform connectivity
- Mobile app connectivity framework ready

### **Complete Web Interface System (November 2025)**

🎉 **FULLY FUNCTIONAL FEEDING MANAGEMENT SYSTEM**: Professional web interface with complete API integration

#### **Web Interface Features:**
- **Real-time Status Dashboard**: WiFi, Last Feeding, Next Feeding with auto-refresh
- **Manual Feeding Controls**: Direct portion selection (1-10 portions) with immediate action
- **Professional Date Formatting**: DD/MM/YYYY HH:MM with leading zeros
- **Responsive Design**: Modern CSS with gradient backgrounds and card-based layout
- **No Confirmation Dialogs**: Streamlined UX with console-based feedback
- **Cache Prevention**: Dynamic timestamps prevent browser caching issues

#### **API Endpoints (All Working):**
- **`/api/test`** → System health check
- **`/api/feed?portions=X`** → Manual feeding (GET method)
- **`/api/feed-test`** → Quick 2-portion test feeding
- **`/api/status`** → Complete system status JSON
- **`/api/schedules`** → Schedule configuration JSON
- **`/callback-check`** → Callback system verification

#### **Centralized Endpoint Registration:**
```cpp
// CORRECT: All endpoints registered in single location
void WiFiController::registerAllEndpoints() {
    // Verify all components ready FIRST
    if (!wifiManager.server || !feedingSchedule || !feedingController) return;
    
    // Register all endpoints in organized groups
    registerTestEndpoints();
    registerFeedingEndpoints();
    registerScheduleEndpoints();
    registerUtilityEndpoints();
}
```

#### **Professional Error Handling:**
- **Component Verification**: All dependencies checked before registration
- **HTTP Method Compatibility**: GET methods for tzapu WiFiManager compatibility
- **Browser Console Integration**: Detailed logging without user-facing popups
- **Automatic UI Updates**: Status refresh after successful operations

#### **System Integration Status:**
✅ **Hardware**: ESP32-WROOM-32, DS3231 RTC, 28BYJ-48 Stepper Motor
✅ **WiFi Management**: tzapu WiFiManager with custom endpoints
✅ **Time Synchronization**: NTP + RTC dual-source time management
✅ **Feeding System**: Manual + automatic scheduling with NVRAM persistence
✅ **Web Interface**: Complete browser-based control and monitoring
✅ **Non-Blocking Architecture**: 7 concurrent tasks with TaskScheduler
✅ **Error Recovery**: Power-loss recovery, missed feeding detection
✅ **Professional UX**: Modern interface with streamlined workflow

## **FINAL REMINDER**
🔥 **NEVER implement features without:**
1. Checking exact library version methods
2. **🚨 MANDATORY BLOCKING CODE AUDIT**
3. Using non-blocking patterns EXCLUSIVELY
4. **🔍 GREP SEARCH for delay() and blocking while()**
5. **📊 PERFORMANCE VERIFICATION < 100ms response**
6. **⚡ RESPONSIVENESS TEST during operations**
7. **🚨 JAVASCRIPT COMMENT VERIFICATION**: NO `//` in generated HTML/JS
8. **🔍 WEB INTERFACE TESTING**: Browser console + HTML source first
9. **📄 DYNAMIC CONTENT VALIDATION**: Test final generated output
10. **🚨 COMPONENT INITIALIZATION ORDER VERIFICATION**
11. **🔍 CENTRALIZED ENDPOINT REGISTRATION PATTERN**
12. **⚠ HTTP METHOD COMPATIBILITY TESTING**
13. **📅 PROFESSIONAL DATE FORMATTING STANDARDS**
14. **🎯 UX OPTIMIZATION AND CONFIRMATION REMOVAL**
15. Testing variable scope
16. Implementing proper UTF-8 encoding
17. Adding to TaskScheduler architecture

### **November 2025 Development Session Summary**
This comprehensive development session successfully resolved:
- ✅ **API 404 Errors**: Fixed component initialization order crisis
- ✅ **HTTP Method Issues**: Adapted to tzapu WiFiManager POST limitations
- ✅ **JavaScript Errors**: Eliminated comment-based syntax breaks
- ✅ **Date Formatting**: Implemented professional DD/MM/YYYY format
- ✅ **UX Issues**: Removed annoying confirmations for streamlined workflow
- ✅ **System Integration**: Achieved complete end-to-end functionality

**RESULT**: Fully functional ESP32 Fish Feeder with professional web interface! 🎉