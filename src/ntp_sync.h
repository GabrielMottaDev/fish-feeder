#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <RTClib.h>
#include <Preferences.h>
#include "config.h"

// Forward declarations
class ModuleManager;
class RTCModule;
class WiFiController;

/**
 * NTPSync Class
 * 
 * Manages automatic time synchronization between internet NTP servers
 * and the local DS3231 RTC module. Provides accurate timekeeping by
 * periodically updating the RTC with internet time.
 * 
 * Features:
 * - Automatic sync every 30 minutes when WiFi is connected
 * - Initial sync shortly after WiFi connection established
 * - Timezone support (configurable GMT offset)
 * - Daylight saving time support
 * - Error handling and retry logic
 * - Status reporting and diagnostics
 * 
 * Architecture:
 * - Uses ModuleManager for accessing RTC and WiFi modules
 * - Reduces coupling between modules
 */
class NTPSync {
private:
    ModuleManager* modules;
    Preferences preferences; // NVRAM storage for last sync timestamp
    
    // Sync state management
    bool ntpInitialized;
    bool syncInProgress;
    unsigned long lastSyncAttempt;
    unsigned long lastSuccessfulSync;
    unsigned long wifiConnectedTime;
    bool initialSyncPending;
    unsigned long syncIntervalMs; // Dynamic sync interval
    bool previousWiFiSleepState; // Store previous WiFi sleep state (true = enabled)
    unsigned long lastSyncTimestampNVRAM; // Last sync timestamp saved in NVRAM
    
    // Non-blocking sync state
    unsigned long syncStartTime;
    unsigned long lastSyncCheck;
    bool waitingForNTPResponse;
    int currentServerIndex;
    bool needsReconfigure;
    
    // Sync statistics
    int syncAttempts;
    int successfulSyncs;
    int failedSyncs;
    
    // HTTP fallback state
    bool httpFallbackInProgress;
    int currentHTTPServerIndex;
    unsigned long httpStartTime;
    
    // Internal helper methods
    bool performNTPSync();
    void configureNTP();
    void configureNTPWithServer(int serverIndex);
    bool waitForNTPSync();
    bool checkNTPSyncProgress(); // New non-blocking method
    void updateRTCFromNTP();
    void printSyncResult(bool success, const String& details = "");
    String formatDateTime(const DateTime& dt);
    const char* getCurrentNTPServer();
    
    // NVRAM helper methods
    void saveLastSyncToNVRAM(unsigned long timestamp);
    unsigned long loadLastSyncFromNVRAM();
    
    // Intelligent sync decision methods
    bool shouldSyncNTP();
    bool isRTCValid();
    bool isRTCOutdated();
    
    // HTTP Time API fallback methods
    bool tryHTTPTimeFallback();
    bool getTimeFromWorldTimeAPI();
    bool getTimeFromTimeAPI();
    bool getTimeFromWorldClockAPI();
    bool parseHTTPTimeResponse(const String& response, DateTime& dateTime);
    bool parseTimeAPIResponse(const String& response, DateTime& dateTime);
    bool setTimeFromUnixTimestamp(unsigned long timestamp, bool applyOffset = true);
    
public:
    // Constructor
    NTPSync(ModuleManager* moduleManager);
    
    // Initialization
    bool begin();
    
    // Main sync operations
    void handleNTPSync();
    bool forceSyncNow();
    void onWiFiConnected();
    
    // Status and information
    bool isNTPInitialized();
    bool isSyncInProgress();
    unsigned long getLastSyncTime();
    unsigned long getTimeSinceLastSync();
    void showSyncStatus();
    void showSyncStatistics();
    
    // Configuration
    void setSyncInterval(unsigned long intervalMs);
    void setTimezone(long gmtOffsetSec, int daylightOffsetSec);
    
    // Command processing
    bool processNTPCommand(const String& command);
};

#endif // NTP_SYNC_H