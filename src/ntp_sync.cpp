#include "ntp_sync.h"
#include "module_manager.h"
#include "rtc_module.h"
#include "wifi_controller.h"
#include "console_manager.h"

/**
 * Constructor: Initialize NTP synchronization module
 */
NTPSync::NTPSync(ModuleManager* moduleManager)
    : modules(moduleManager), ntpInitialized(false),
      syncInProgress(false), lastSyncAttempt(0), lastSuccessfulSync(0),
      wifiConnectedTime(0), initialSyncPending(false),
      syncStartTime(0), lastSyncCheck(0), waitingForNTPResponse(false),
      currentServerIndex(0), needsReconfigure(true),
      syncAttempts(0), successfulSyncs(0), failedSyncs(0),
      httpFallbackInProgress(false), currentHTTPServerIndex(0), httpStartTime(0),
      syncIntervalMs(NTP_SYNC_INTERVAL), previousWiFiSleepState(false) {
}

/**
 * Initialize NTP synchronization system
 */
bool NTPSync::begin() {
    Console::printlnR(F("=== NTP Time Synchronization Initialization ==="));
    
    if (!modules || !modules->hasRTCModule()) {
        Console::printlnR(F("ERROR: RTC module not available for NTP sync"));
        return false;
    }
    
    Console::printR(F("Available Time Servers ("));
    Console::printR(String(TIME_SERVERS_COUNT));
    Console::printlnR(F("):"));
    
    for (int i = 0; i < TIME_SERVERS_COUNT; i++) {
        Console::printR(F("  "));
        Console::printR(String(i + 1));
        Console::printR(F(". ["));
        Console::printR(TIME_SERVERS[i].type);
        Console::printR(F("] "));
        Console::printR(TIME_SERVERS[i].server);
        if (i == 0) {
            Console::printlnR(F(" (primary)"));
        } else {
            Console::printlnR(F(" (fallback)"));
        }
    }
    
    Console::printR(F("GMT Offset: "));
    Console::printR(String(GMT_OFFSET_SEC / 3600));
    Console::printlnR(F(" hours"));
    Console::printR(F("Sync Interval: "));
    Console::printR(String(syncIntervalMs / 60000));
    Console::printlnR(F(" minutes"));
    Console::printR(F("Sync Timeout: "));
    Console::printR(String(NTP_SYNC_TIMEOUT / 1000));
    Console::printlnR(F(" seconds"));
    Console::printlnR(F("NTP synchronization module initialized"));
    Console::printlnR(F("Will sync automatically when WiFi is connected"));
    Console::printlnR(F("==============================================="));
    
    return true;
}

/**
 * Handle periodic NTP synchronization
 * Called regularly by task scheduler - Non-blocking implementation
 */
void NTPSync::handleNTPSync() {
    // Skip if WiFi not connected
    if (!modules->getWiFiController()->isWiFiConnected()) {
        // Reset sync state if WiFi disconnected during sync
        if (syncInProgress) {
            syncInProgress = false;
            waitingForNTPResponse = false;
            Console::printlnR(F("NTP sync cancelled - WiFi disconnected"));
        }
        return;
    }
    
    // If sync is in progress, check for completion
    if (syncInProgress && waitingForNTPResponse) {
        if (checkNTPSyncProgress()) {
            // Sync completed (success or failure handled in checkNTPSyncProgress)
            return;
        }
        // Still waiting, don't start new sync
        return;
    }
    
    // Handle initial sync after WiFi connection
    if (initialSyncPending && (millis() - wifiConnectedTime > NTP_INITIAL_SYNC_DELAY)) {
        Console::printlnR(F("Performing initial NTP synchronization..."));
        forceSyncNow();
        initialSyncPending = false;
        return;
    }
    
    // Handle periodic sync
    if (lastSuccessfulSync > 0 && (millis() - lastSuccessfulSync > syncIntervalMs)) {
        Console::printlnR(F("Performing scheduled NTP synchronization..."));
        forceSyncNow();
    }
}

/**
 * Force immediate NTP synchronization - Non-blocking
 */
bool NTPSync::forceSyncNow() {
    if (syncInProgress) {
        Console::printlnR(F("NTP sync already in progress..."));
        return false;
    }
    
    if (!modules->getWiFiController()->isWiFiConnected()) {
        Console::printlnR(F("Cannot sync NTP: WiFi not connected"));
        return false;
    }
    
    Console::printlnR(F("Starting NTP synchronization (non-blocking)..."));
    lastSyncAttempt = millis();
    syncAttempts++;
    
    // Start non-blocking sync
    performNTPSync();
    
    // Return true to indicate sync was started (not completed)
    return true;
}

/**
 * Called when WiFi connection is established
 */
void NTPSync::onWiFiConnected() {
    Console::printlnR(F("WiFi connected - scheduling initial NTP sync"));
    wifiConnectedTime = millis();
    initialSyncPending = true;
}

/**
 * Perform actual NTP synchronization - Non-blocking version
 */
bool NTPSync::performNTPSync() {
    syncInProgress = true;
    waitingForNTPResponse = true;
    syncStartTime = millis();
    lastSyncCheck = 0;
    
    // 🚨 CRITICAL: Disable WiFi sleep during NTP sync for reliable UDP packets
    previousWiFiSleepState = WiFi.getSleep();
    if (previousWiFiSleepState) {
        WiFi.setSleep(false); // Disable sleep mode
        Console::printlnR(F("WiFi sleep disabled for NTP sync"));
    }
    
    // Configure NTP if not already done or needs reconfigure
    if (!ntpInitialized || needsReconfigure) {
        configureNTP();
    }
    
    Console::printR(F("Starting NTP sync with "));
    Console::printR(getCurrentNTPServer());
    Console::printR(F(" (non-blocking)"));
    
    // Start the sync process - this is non-blocking
    // The actual waiting and result checking will happen in checkNTPSyncProgress()
    return true; // Always return true for non-blocking start
}

/**
 * Configure NTP client with primary server
 */
void NTPSync::configureNTP() {
    configureNTPWithServer(0); // Start with primary server
}

/**
 * Wait for NTP synchronization to complete - LEGACY BLOCKING VERSION
 * This method is kept for compatibility but should not be used in main loop
 */
bool NTPSync::waitForNTPSync() {
    Console::printR(F("Waiting for NTP sync"));
    
    unsigned long startTime = millis();
    struct tm timeinfo;
    
    while (millis() - startTime < NTP_SYNC_TIMEOUT) {
        if (getLocalTime(&timeinfo)) {
            Console::printlnR(F(" ✓"));
            return true;
        }
        Console::printR(F("."));
        delay(500);
    }
    
    Console::printlnR(F(" ✗"));
    return false;
}

/**
 * Check NTP synchronization progress - NON-BLOCKING VERSION
 * Called periodically to check if NTP sync has completed
 * Uses intercalated TIME_SERVERS array (NTP and HTTP mixed)
 */
bool NTPSync::checkNTPSyncProgress() {
    if (!waitingForNTPResponse) {
        return false; // Not waiting for anything
    }
    
    // Check for timeout
    if (millis() - syncStartTime > NTP_SYNC_TIMEOUT) {
        // Timeout reached - try next server
        Console::printlnR(F(" ✗"));
        
        // Try next server if available
        if (currentServerIndex < TIME_SERVERS_COUNT - 1) {
            currentServerIndex++;
            
            // Check server type and handle accordingly
            const TimeServerEntry& entry = TIME_SERVERS[currentServerIndex];
            
            if (strcmp(entry.type, "http") == 0) {
                // HTTP time server - try immediately
                Console::printR(F("Trying HTTP server: "));
                Console::printlnR(entry.server);
                
                // Try HTTP time request (synchronous for simplicity)
                if (tryHTTPTimeFallback()) {
                    successfulSyncs++;
                    lastSuccessfulSync = millis();
                    syncInProgress = false;
                    waitingForNTPResponse = false;
                    needsReconfigure = true;
                    currentServerIndex = 0; // Reset for next sync
                    
                    // Restore WiFi sleep mode
                    if (previousWiFiSleepState) {
                        WiFi.setSleep(true);
                        Console::printlnR(F("WiFi sleep mode restored"));
                    }
                    
                    printSyncResult(true, String("HTTP time from ") + entry.server);
                    return true; // Sync completed (success via HTTP)
                }
                
                // HTTP failed, continue to next server
                syncStartTime = millis();
                lastSyncCheck = 0;
                return false; // Continue with next server
                
            } else {
                // NTP server - reconfigure and retry
                Console::printR(F("Trying NTP server: "));
                Console::printlnR(entry.server);
                
                // Reconfigure with new NTP server
                configureNTPWithServer(currentServerIndex);
                
                // Reset timing for next attempt
                syncStartTime = millis();
                lastSyncCheck = 0;
                Console::printR(F("Retrying NTP sync"));
                
                return false; // Continue with next server
            }
        } else {
            // All servers failed (NTP and HTTP intercalated)
            Console::printlnR(F("All time servers failed (NTP and HTTP)"));
            
            currentServerIndex = 0; // Reset for next attempt
            failedSyncs++;
            syncInProgress = false;
            waitingForNTPResponse = false;
            needsReconfigure = true;
            
            // Restore WiFi sleep mode after failure
            if (previousWiFiSleepState) {
                WiFi.setSleep(true);
                Console::printlnR(F("WiFi sleep mode restored"));
            }
            
            String failureMsg = "All ";
            failureMsg += String(TIME_SERVERS_COUNT);
            failureMsg += " time servers failed";
            
            printSyncResult(false, failureMsg);
            return true; // Sync completed (failed)
        }
    }
    
    // Check every 500ms to avoid too frequent checks
    if (millis() - lastSyncCheck < 500) {
        return false; // Not time to check yet
    }
    
    lastSyncCheck = millis();
    
    // Check if current server is NTP (skip for HTTP entries)
    const TimeServerEntry& entry = TIME_SERVERS[currentServerIndex];
    if (strcmp(entry.type, "http") == 0) {
        // Already handled HTTP in timeout section, skip checking here
        return false;
    }
    
    // Try to get time from NTP
    struct tm timeinfo;
    Console::printR(F("Checking NTP response from "));
    Console::printR(entry.server);
    Console::printR(F("... "));
    
    if (getLocalTime(&timeinfo)) {
        // Success! NTP sync completed
        successfulSyncs++;
        lastSuccessfulSync = millis();
        syncInProgress = false;
        waitingForNTPResponse = false;
        currentServerIndex = 0; // Reset to primary server for next sync
        
        // Restore WiFi sleep mode after successful sync
        if (previousWiFiSleepState) {
            WiFi.setSleep(true);
            Console::printlnR(F("WiFi sleep mode restored"));
        }
        
        Console::printlnR(F(" ✓"));
        
        // Show received time
        Console::printR(F("Received NTP time: "));
        Console::printR(String(timeinfo.tm_mday));
        Console::printR(F("/"));
        Console::printR(String(timeinfo.tm_mon + 1));
        Console::printR(F("/"));
        Console::printR(String(timeinfo.tm_year + 1900));
        Console::printR(F(" "));
        Console::printR(String(timeinfo.tm_hour));
        Console::printR(F(":"));
        Console::printR(String(timeinfo.tm_min));
        Console::printR(F(":"));
        Console::printlnR(String(timeinfo.tm_sec));
        
        String successMsg = "Time synchronized with ";
        successMsg += entry.server;
        
        updateRTCFromNTP();
        printSyncResult(true, successMsg);
        return true; // Sync completed (success)
    } else {
        Console::printlnR(F("No response yet"));
    }
    
    // Still waiting - show progress dot
    Console::printR(F("."));
    return false; // Still in progress
}

/**
 * Update RTC module with time from NTP
 */
void NTPSync::updateRTCFromNTP() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Console::printlnR(F("ERROR: Could not obtain time from NTP"));
        return;
    }
    
    // Convert tm struct to DateTime
    DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Get current RTC time for comparison
    DateTime rtcTime = modules->getRTCModule()->now();
    
    Console::printR(F("NTP Time: "));
    Console::printlnR(formatDateTime(ntpTime));
    Console::printR(F("RTC Time: "));
    Console::printlnR(formatDateTime(rtcTime));
    
    // Calculate time difference
    int32_t timeDiff = ntpTime.unixtime() - rtcTime.unixtime();
    Console::printR(F("Time difference: "));
    Console::printR(String(timeDiff));
    Console::printlnR(F(" seconds"));
    
    // Update RTC if difference is significant (more than 2 seconds)
    if (abs(timeDiff) > 2) {
        modules->getRTCModule()->adjust(ntpTime);
        Console::printlnR(F("RTC updated with NTP time"));
    } else {
        Console::printlnR(F("RTC time is already accurate (no update needed)"));
    }
}

/**
 * Print synchronization result
 */
void NTPSync::printSyncResult(bool success, const String& details) {
    Console::printR(F("NTP Sync Result: "));
    Console::printR(success ? F("SUCCESS") : F("FAILED"));
    if (details.length() > 0) {
        Console::printR(F(" - "));
        Console::printR(details);
    }
    Console::printlnR(F(""));
}

/**
 * Format DateTime for display
 */
String NTPSync::formatDateTime(const DateTime& dt) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d:%02d",
             dt.day(), dt.month(), dt.year(),
             dt.hour(), dt.minute(), dt.second());
    return String(buffer);
}

/**
 * Check if NTP is initialized
 */
bool NTPSync::isNTPInitialized() {
    return ntpInitialized;
}

/**
 * Check if sync is in progress
 */
bool NTPSync::isSyncInProgress() {
    return syncInProgress;
}

/**
 * Get last successful sync time
 */
unsigned long NTPSync::getLastSyncTime() {
    return lastSuccessfulSync;
}

/**
 * Get time since last successful sync
 */
unsigned long NTPSync::getTimeSinceLastSync() {
    if (lastSuccessfulSync == 0) {
        return 0;
    }
    return millis() - lastSuccessfulSync;
}

/**
 * Show current sync status
 */
void NTPSync::showSyncStatus() {
    Console::printlnR(F(""));
    Console::printlnR(F("=== NTP SYNCHRONIZATION STATUS ==="));
    Console::printR(F("NTP Initialized: "));
    Console::printlnR(ntpInitialized ? F("Yes") : F("No"));
    
    if (ntpInitialized) {
        Console::printR(F("Current NTP Server: "));
        Console::printlnR(getCurrentNTPServer());
    }
    
    Console::printR(F("Sync in Progress: "));
    Console::printlnR(syncInProgress ? F("Yes") : F("No"));
    
    if (syncInProgress && waitingForNTPResponse) {
        Console::printR(F("Sync Status: Waiting for "));
        Console::printR(getCurrentNTPServer());
        Console::printR(F(" ("));
        Console::printR(String((millis() - syncStartTime) / 1000));
        Console::printR(F("s / "));
        Console::printR(String(NTP_SYNC_TIMEOUT / 1000));
        Console::printlnR(F("s)"));
    }
    
    Console::printR(F("WiFi Connected: "));
    Console::printlnR(modules->getWiFiController()->isWiFiConnected() ? F("Yes") : F("No"));
    
    if (lastSuccessfulSync > 0) {
        Console::printR(F("Last Successful Sync: "));
        Console::printR(String((millis() - lastSuccessfulSync) / 60000));
        Console::printlnR(F(" minutes ago"));
    } else {
        Console::printlnR(F("Last Successful Sync: Never"));
    }
    
    if (initialSyncPending) {
        Console::printlnR(F("Initial sync pending after WiFi connection"));
    }
    
    Console::printR(F("Next Sync: "));
    if (lastSuccessfulSync > 0) {
        unsigned long timeSinceLastSync = millis() - lastSuccessfulSync;
        // Proper overflow-safe calculation
        if (timeSinceLastSync >= syncIntervalMs) {
            Console::printlnR(F("Due now"));
        } else {
            unsigned long nextSync = syncIntervalMs - timeSinceLastSync;
            Console::printR(String(nextSync / 60000));
            Console::printlnR(F(" minutes"));
        }
    } else {
        Console::printlnR(F("Waiting for WiFi connection"));
    }
    
    Console::printlnR(F("=================================="));
}

/**
 * Show synchronization statistics
 */
void NTPSync::showSyncStatistics() {
    Console::printlnR(F(""));
    Console::printlnR(F("=== NTP SYNC STATISTICS ==="));
    Console::printR(F("Total Attempts: "));
    Console::printlnR(String(syncAttempts));
    Console::printR(F("Successful Syncs: "));
    Console::printlnR(String(successfulSyncs));
    Console::printR(F("Failed Syncs: "));
    Console::printlnR(String(failedSyncs));
    
    if (syncAttempts > 0) {
        float successRate = (float)successfulSyncs / syncAttempts * 100.0;
        Console::printR(F("Success Rate: "));
        Console::printR(String(successRate, 1));
        Console::printlnR(F("%"));
    }
    
    Console::printlnR(F("==========================="));
}

/**
 * Set custom sync interval
 */
void NTPSync::setSyncInterval(unsigned long intervalMs) {
    syncIntervalMs = intervalMs; // 🚨 FIX: Actually update the interval variable!
    Console::printR(F("NTP sync interval updated to "));
    Console::printR(String(syncIntervalMs / 60000));
    Console::printlnR(F(" minutes"));
}

/**
 * Set custom timezone
 */
void NTPSync::setTimezone(long gmtOffsetSec, int daylightOffsetSec) {
    Console::printR(F("Timezone updated: GMT"));
    Console::printR(gmtOffsetSec >= 0 ? F("+") : F(""));
    Console::printR(String(gmtOffsetSec / 3600));
    Console::printlnR(F(" hours"));
    
    // Reconfigure NTP with new timezone using current server
    configTime(gmtOffsetSec, daylightOffsetSec, getCurrentNTPServer());
}

/**
 * Get current time server name from intercalated array
 */
const char* NTPSync::getCurrentNTPServer() {
    if (currentServerIndex >= 0 && currentServerIndex < TIME_SERVERS_COUNT) {
        return TIME_SERVERS[currentServerIndex].server;
    }
    return TIME_SERVERS[0].server; // Default to first server
}

/**
 * Configure NTP with specific server from intercalated array
 */
void NTPSync::configureNTPWithServer(int serverIndex) {
    currentServerIndex = serverIndex;
    
    // Get entry from intercalated array
    if (serverIndex >= TIME_SERVERS_COUNT) {
        Console::printlnR(F("ERROR: Invalid server index"));
        return;
    }
    
    const TimeServerEntry& entry = TIME_SERVERS[serverIndex];
    
    // Only configure NTP for NTP servers, skip HTTP servers
    if (strcmp(entry.type, "http") == 0) {
        Console::printR(F("Skipping NTP configuration for HTTP server: "));
        Console::printlnR(entry.server);
        return;
    }
    
    Console::printR(F("Configuring NTP with server: "));
    Console::printlnR(entry.server);
    Console::printR(F("GMT Offset: "));
    Console::printR(String(GMT_OFFSET_SEC));
    Console::printR(F(" seconds ("));
    Console::printR(String(GMT_OFFSET_SEC / 3600));
    Console::printlnR(F(" hours)"));
    
    // Test DNS resolution first
    IPAddress serverIP;
    if (WiFi.hostByName(entry.server, serverIP)) {
        Console::printR(F("DNS resolved "));
        Console::printR(entry.server);
        Console::printR(F(" to "));
        Console::printlnR(serverIP.toString());
    } else {
        Console::printR(F("⚠ DNS resolution failed for "));
        Console::printlnR(entry.server);
    }
    
    // Configure NTP with timezone settings and specific server
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, entry.server);
    
    Console::printlnR(F("✓ NTP configuration completed"));
    
    ntpInitialized = true;
    needsReconfigure = false;
    Console::printlnR(F("NTP client configured"));
}

/**
 * Process NTP-related commands
 */
bool NTPSync::processNTPCommand(const String& command) {
    if (command == "NTP STATUS") {
        showSyncStatus();
        return true;
    }
    else if (command == "NTP STATS") {
        showSyncStatistics();
        return true;
    }
    else if (command == "NTP SYNC") {
        forceSyncNow();
        return true;
    }
    else if (command == "NTP FALLBACK") {
        Console::printlnR(F("Testing HTTP time fallback..."));
        if (tryHTTPTimeFallback()) {
            Console::printlnR(F("✓ HTTP time fallback successful"));
        } else {
            Console::printlnR(F("✗ HTTP time fallback failed"));
        }
        return true;
    }
    else if (command.startsWith("NTP INTERVAL ")) {
        String intervalStr = command.substring(13);
        int minutes = intervalStr.toInt();
        if (minutes > 0) {
            setSyncInterval(minutes * 60000);
            return true;
        } else {
            Console::printlnR(F("Usage: NTP INTERVAL [minutes]"));
            return true;
        }
    }
    
    return false;
}

/**
 * Try HTTP time fallback when NTP fails
 * Uses current server from intercalated TIME_SERVERS array
 */
bool NTPSync::tryHTTPTimeFallback() {
    // Get current server from intercalated array
    if (currentServerIndex >= TIME_SERVERS_COUNT) {
        Console::printlnR(F("ERROR: Invalid server index for HTTP fallback"));
        return false;
    }
    
    const TimeServerEntry& entry = TIME_SERVERS[currentServerIndex];
    
    // Verify it's an HTTP server
    if (strcmp(entry.type, "http") != 0) {
        Console::printlnR(F("ERROR: Current server is not HTTP type"));
        return false;
    }
    
    Console::printR(F("Trying HTTP time server: "));
    Console::printlnR(entry.server);
    
    bool success = false;
    
    // Determine which HTTP time API based on server URL
    String serverStr = String(entry.server);
    
    if (serverStr.indexOf("worldtimeapi.org") >= 0) {
        success = getTimeFromWorldTimeAPI();
    } else if (serverStr.indexOf("timeapi.io") >= 0) {
        success = getTimeFromTimeAPI();
    } else if (serverStr.indexOf("worldclockapi.com") >= 0) {
        success = getTimeFromWorldClockAPI(); // Separate handler for UTC API
    } else {
        Console::printlnR(F("Unknown HTTP time API format"));
        return false;
    }
    
    if (success) {
        Console::printR(F("✓ HTTP time sync successful with "));
        Console::printlnR(entry.server);
        return true;
    } else {
        Console::printR(F("✗ HTTP time sync failed with "));
        Console::printlnR(entry.server);
        return false;
    }
}

/**
 * Get time from WorldTimeAPI (worldtimeapi.org)
 * Free API that provides JSON time data
 * 
 * 🚨 CRITICAL CORRECTION: WorldTimeAPI "unixtime" field is ALWAYS UTC!
 * The "datetime" field has local time, but "unixtime" is UTC timestamp.
 * We must parse the "datetime" field with timezone offset instead.
 */
bool NTPSync::getTimeFromWorldTimeAPI() {
    WiFiClient client;
    const char* host = "worldtimeapi.org";
    const int httpPort = 80;
    
    Console::printR(F("Connecting to "));
    Console::printR(host);
    Console::printR(F(":"));
    Console::printlnR(String(httpPort));
    
    if (!client.connect(host, httpPort)) {
        Console::printlnR(F("Connection to WorldTimeAPI failed"));
        return false;
    }
    
    // Make HTTP request for Brazil timezone
    String url = "/api/timezone/America/Sao_Paulo";
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    
    // Wait for response with timeout (non-blocking)
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > HTTP_TIME_TIMEOUT) {
            Console::printlnR(F("WorldTimeAPI timeout"));
            client.stop();
            return false;
        }
        yield(); // Non-blocking wait
    }
    
    // Read response
    String response = "";
    while (client.available()) {
        response += client.readStringUntil('\r');
    }
    client.stop();
    
    Console::printlnR(F("WorldTimeAPI response received"));
    
    // Try to parse "datetime" field first (has timezone already applied)
    // Format: "datetime":"2025-11-06T00:37:27.123456-03:00"
    int datetimePos = response.indexOf("\"datetime\":");
    if (datetimePos != -1) {
        int startPos = response.indexOf('"', datetimePos + 11) + 1;
        int endPos = response.indexOf('"', startPos);
        
        if (endPos != -1) {
            String datetimeStr = response.substring(startPos, endPos);
            Console::printR(F("Found datetime: "));
            Console::printlnR(datetimeStr);
            
            // Parse ISO format with timezone: YYYY-MM-DDTHH:MM:SS±HH:MM
            if (datetimeStr.length() >= 19) {
                int year = datetimeStr.substring(0, 4).toInt();
                int month = datetimeStr.substring(5, 7).toInt();
                int day = datetimeStr.substring(8, 10).toInt();
                int hour = datetimeStr.substring(11, 13).toInt();
                int minute = datetimeStr.substring(14, 16).toInt();
                int second = datetimeStr.substring(17, 19).toInt();
                
                Console::printR(F("Parsed: "));
                Console::printR(String(day));
                Console::printR(F("/"));
                Console::printR(String(month));
                Console::printR(F("/"));
                Console::printR(String(year));
                Console::printR(F(" "));
                Console::printR(String(hour));
                Console::printR(F(":"));
                Console::printR(String(minute));
                Console::printR(F(":"));
                Console::printlnR(String(second));
                
                // Create DateTime (already local time with timezone applied by API)
                DateTime dt(year, month, day, hour, minute, second);
                
                // Update RTC
                modules->getRTCModule()->adjust(dt);
                Console::printlnR(F("✓ RTC updated from WorldTimeAPI (local time)"));
                return true;
            }
        }
    }
    
    Console::printlnR(F("Failed to parse WorldTimeAPI datetime field"));
    return false;
}

/**
 * Get time from TimeAPI.io
 * Alternative free time API
 * 
 * 🚨 TimeAPI returns datetime in requested timezone format
 */
bool NTPSync::getTimeFromTimeAPI() {
    WiFiClient client;
    const char* host = "timeapi.io";
    const int httpPort = 80;
    
    Console::printR(F("Connecting to "));
    Console::printR(host);
    Console::printR(F(":"));
    Console::printlnR(String(httpPort));
    
    if (!client.connect(host, httpPort)) {
        Console::printlnR(F("Connection to TimeAPI failed"));
        return false;
    }
    
    // Make HTTP request for Brazil timezone
    String url = "/api/Time/current/zone?timeZone=America/Sao_Paulo";
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    
    // Wait for response with timeout (non-blocking)
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > HTTP_TIME_TIMEOUT) {
            Console::printlnR(F("TimeAPI timeout"));
            client.stop();
            return false;
        }
        yield(); // Non-blocking wait
    }
    
    // Read response
    String response = "";
    while (client.available()) {
        response += client.readStringUntil('\r');
    }
    client.stop();
    
    Console::printlnR(F("TimeAPI response received"));
    
    // Parse datetime field (should have timezone already applied)
    DateTime parsedTime;
    if (parseTimeAPIResponse(response, parsedTime)) {
        // Update RTC with parsed local time
        modules->getRTCModule()->adjust(parsedTime);
        Console::printlnR(F("✓ RTC updated from TimeAPI (local time)"));
        return true;
    }
    
    Console::printlnR(F("Failed to parse TimeAPI response"));
    return false;
}

/**
 * Parse HTTP time response and extract DateTime
 * Handles both ISO 8601 with timezone offset and without
 */
bool NTPSync::parseHTTPTimeResponse(const String& response, DateTime& dateTime) {
    // This function is deprecated - use specific API parsers instead
    return false;
}

/**
 * Parse TimeAPI.io response
 * TimeAPI returns datetime in requested timezone
 */
bool NTPSync::parseTimeAPIResponse(const String& response, DateTime& dateTime) {
    // Look for "dateTime" field
    int datePos = response.indexOf("\"dateTime\":");
    if (datePos == -1) {
        return false;
    }
    
    int startPos = response.indexOf('"', datePos + 11) + 1;
    int endPos = response.indexOf('"', startPos);
    
    if (endPos != -1) {
        String dateTimeStr = response.substring(startPos, endPos);
        Console::printR(F("Found dateTime: "));
        Console::printlnR(dateTimeStr);
        
        // Parse ISO format: YYYY-MM-DDTHH:MM:SS
        if (dateTimeStr.length() >= 19) {
            int year = dateTimeStr.substring(0, 4).toInt();
            int month = dateTimeStr.substring(5, 7).toInt();
            int day = dateTimeStr.substring(8, 10).toInt();
            int hour = dateTimeStr.substring(11, 13).toInt();
            int minute = dateTimeStr.substring(14, 16).toInt();
            int second = dateTimeStr.substring(17, 19).toInt();
            
            Console::printR(F("Parsed: "));
            Console::printR(String(day));
            Console::printR(F("/"));
            Console::printR(String(month));
            Console::printR(F("/"));
            Console::printR(String(year));
            Console::printR(F(" "));
            Console::printR(String(hour));
            Console::printR(F(":"));
            Console::printR(String(minute));
            Console::printR(F(":"));
            Console::printlnR(String(second));
            
            // TimeAPI with timezone parameter returns local time
            dateTime = DateTime(year, month, day, hour, minute, second);
            return true;
        }
    }
    
    return false;
}

/**
 * Get time from WorldClockAPI (worldclockapi.com)
 * This API returns UTC time, so we must apply GMT offset
 */
bool NTPSync::getTimeFromWorldClockAPI() {
    WiFiClient client;
    const char* host = "worldclockapi.com";
    const int httpPort = 80;
    
    Console::printR(F("Connecting to "));
    Console::printR(host);
    Console::printR(F(":"));
    Console::printlnR(String(httpPort));
    
    if (!client.connect(host, httpPort)) {
        Console::printlnR(F("Connection to WorldClockAPI failed"));
        return false;
    }
    
    // Make HTTP request for UTC time
    String url = "/api/json/utc/now";
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    
    // Wait for response with timeout (non-blocking)
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > HTTP_TIME_TIMEOUT) {
            Console::printlnR(F("WorldClockAPI timeout"));
            client.stop();
            return false;
        }
        yield(); // Non-blocking wait
    }
    
    // Read response
    String response = "";
    while (client.available()) {
        response += client.readStringUntil('\r');
    }
    client.stop();
    
    Console::printlnR(F("WorldClockAPI response received"));
    
    // Parse "currentDateTime" field (UTC time with Z suffix)
    int datePos = response.indexOf("\"currentDateTime\":");
    if (datePos != -1) {
        int startPos = response.indexOf('"', datePos + 18) + 1;
        int endPos = response.indexOf('"', startPos);
        
        if (endPos != -1) {
            String dateTimeStr = response.substring(startPos, endPos);
            Console::printR(F("Found currentDateTime: "));
            Console::printlnR(dateTimeStr);
            
            // Parse ISO format: YYYY-MM-DDTHH:MMZ
            if (dateTimeStr.length() >= 16) {
                int year = dateTimeStr.substring(0, 4).toInt();
                int month = dateTimeStr.substring(5, 7).toInt();
                int day = dateTimeStr.substring(8, 10).toInt();
                int hour = dateTimeStr.substring(11, 13).toInt();
                int minute = dateTimeStr.substring(14, 16).toInt();
                int second = 0; // WorldClockAPI doesn't include seconds
                
                Console::printR(F("Parsed UTC: "));
                Console::printR(String(day));
                Console::printR(F("/"));
                Console::printR(String(month));
                Console::printR(F("/"));
                Console::printR(String(year));
                Console::printR(F(" "));
                Console::printR(String(hour));
                Console::printR(F(":"));
                Console::printR(String(minute));
                Console::printR(F(":"));
                Console::printlnR(String(second));
                
                // Create DateTime with UTC time
                DateTime utcTime(year, month, day, hour, minute, second);
                
                // Convert UTC to local time by applying GMT offset
                unsigned long utcTimestamp = utcTime.unixtime();
                unsigned long localTimestamp = utcTimestamp + GMT_OFFSET_SEC;
                DateTime localTime(localTimestamp);
                
                Console::printR(F("Converted to local: "));
                Console::printlnR(formatDateTime(localTime));
                
                // Update RTC with local time
                modules->getRTCModule()->adjust(localTime);
                Console::printlnR(F("✓ RTC updated from WorldClockAPI (UTC + offset)"));
                return true;
            }
        }
    }
    
    Console::printlnR(F("Failed to parse WorldClockAPI response"));
    return false;
}

/**
 * Set time from Unix timestamp
 * 
 * @param timestamp: Unix timestamp (seconds since 1970-01-01)
 * @param applyOffset: true = timestamp is UTC, apply GMT offset
 *                     false = timestamp is already local time
 */
bool NTPSync::setTimeFromUnixTimestamp(unsigned long timestamp, bool applyOffset) {
    if (timestamp < 1000000000) { // Sanity check (must be after 2001)
        Console::printlnR(F("Invalid timestamp"));
        return false;
    }
    
    unsigned long localTimestamp;
    
    if (applyOffset) {
        // Timestamp is UTC - apply GMT offset to convert to local time
        // GMT_OFFSET_SEC is negative for Brazil (UTC-3 = -10800 seconds)
        localTimestamp = timestamp + GMT_OFFSET_SEC;
        
        Console::printR(F("UTC timestamp: "));
        Console::printlnR(String(timestamp));
        Console::printR(F("Applying offset (UTC"));
        Console::printR(String(GMT_OFFSET_SEC / 3600));
        Console::printR(F("): "));
        Console::printlnR(String(localTimestamp));
    } else {
        // Timestamp is already local time - use directly
        localTimestamp = timestamp;
        
        Console::printR(F("Local timestamp (already with offset): "));
        Console::printlnR(String(localTimestamp));
    }
    
    // Convert to DateTime
    DateTime dt(localTimestamp);
    
    Console::printR(F("Converted to: "));
    Console::printlnR(formatDateTime(dt));
    
    // Update RTC
    modules->getRTCModule()->adjust(dt);
    Console::printlnR(F("✓ RTC updated from HTTP timestamp"));
    return true;
}
