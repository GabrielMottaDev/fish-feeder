#include "wifi_controller.h"
#include "module_manager.h"
#include "feeding_schedule.h"
#include "feeding_controller.h"
#include "console_manager.h"
#include "config.h"
#include <RTClib.h>

/**
 * Constructor: Initialize WiFi Controller
 */
WiFiController::WiFiController() 
    : wifiEnabled(false), isConnected(false), lastConnectionAttempt(0),
      configPortalActive(false), lastConnectionCheck(0), wasConnectedBefore(false),
      portalStartRequested(false), portalAPName(""), portalStartTime(0), shutdownRequested(false),
      connectionState(WIFI_IDLE), connectionStateTime(0), connectionAttempts(0),
      pendingSSID(""), pendingPassword(""), pendingSaveCredentials(false),
      modules(nullptr) {
}

/**
 * Initialize WiFi Controller
 */
bool WiFiController::begin() {
    Console::printlnR(F("=== WiFi Controller Initialization ==="));
    
    // Initialize Preferences for storing network credentials
    if (!preferences.begin("wifi_creds", false)) {
        Console::printlnR(F("ERROR: Failed to initialize preferences storage"));
        return false;
    }
    
    // Initialize WiFi in AP+STA mode for always-on portal
    WiFi.mode(WIFI_AP_STA);
    wifiEnabled = true;
    
    // Initialize tzapu WiFiManager with configuration-based settings for persistent portal
    wifiManager.setDebugOutput(false); // Disable debug to keep console clean
    wifiManager.setConfigPortalTimeout(0); // 0 = NEVER timeout - portal stays active forever
    wifiManager.setConnectTimeout(WIFI_CONNECTION_TIMEOUT / 1000); // Convert to seconds
    
    Console::printR(F("WiFi MAC Address: "));
    Console::printlnR(WiFi.macAddress());
    Console::printlnR(F("WiFi Controller initialized successfully"));
    Console::printlnR(F("tzapu WiFiManager integration ready"));
    Console::printlnR(F("Portal timeout: NEVER (always active)"));
    Console::printlnR(F("Portal mode: Always-On AP+STA"));
    Console::printlnR(F("Portal blocking: DISABLED (non-blocking)"));
    Console::printR(F("Auto-start on boot: "));
    Console::printlnR(WIFI_PORTAL_AUTO_START ? F("ENABLED") : F("DISABLED"));
    
    // Start always-on portal first
    Console::printlnR(F("Starting always-on WiFi configuration portal..."));
    startAlwaysOnPortal();
    
    // Try to auto-connect to saved networks while portal runs
    Console::printlnR(F("Attempting auto-connection to saved networks..."));
    if (tryAutoConnect()) {
        Console::printlnR(F("✓ Successfully connected to saved network"));
        // Configure DNS servers automatically on successful connection
        configureDNSServers();
    } else {
        Console::printlnR(F("No saved networks available - portal remains active for configuration"));
    }

    Console::printlnR(F("===================================="));
    
    return true;
}

/**
 * Set reference to ModuleManager for accessing other system components
 */
void WiFiController::setModuleManager(ModuleManager* moduleManager) {
    modules = moduleManager;
    Console::printlnR(F("WiFiController: ModuleManager reference configured"));
}

/**
 * Register ALL endpoints in the correct order after all components are initialized
 * This is the ONLY place where endpoints should be registered to avoid duplicates
 */
void WiFiController::registerAllEndpoints() {
    Console::printlnR("=== REGISTERING ALL ENDPOINTS (FINAL) ===");
    Console::printlnR("Checking system components...");
    
    // Verify all components are available
    Console::printlnR("✓ Server: " + String(wifiManager.server ? "Available" : "NULL"));
    Console::printlnR("✓ modules->getFeedingSchedule(): " + String(modules && modules->hasFeedingSchedule() ? "Available" : "NULL"));
    Console::printlnR("✓ modules->getFeedingController(): " + String(modules && modules->hasFeedingController() ? "Available" : "NULL"));
    
    if (!wifiManager.server) {
        Console::printlnR("❌ ERROR: Web server not available");
        return;
    }
    
    if (!modules || !modules->getFeedingSchedule() || !modules->getFeedingController()) {
        Console::printlnR("❌ ERROR: Components not ready for endpoint registration");
        return;
    }
    
    Console::printlnR("=== REGISTERING CORE ENDPOINTS ===");
    
    // 1. Test endpoints (always working)
    wifiManager.server->on("/api/test", HTTP_GET, [this]() {
        wifiManager.server->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"API endpoint working\"}");
    });
    Console::printlnR("✓ Registered: /api/test");
    
    wifiManager.server->on("/api/feed-test", HTTP_GET, [this]() {
        if (modules && modules->getFeedingController() && modules->getFeedingController()->isReady()) {
            modules->getFeedingController()->dispenseFoodAsync(2);
            wifiManager.server->send(200, "application/json", "{\"success\":true,\"message\":\"Test feeding started (2 portions)\"}");
        } else {
            wifiManager.server->send(500, "application/json", "{\"success\":false,\"message\":\"Feeding controller not ready\"}");
        }
    });
    Console::printlnR("✓ Registered: /api/feed-test");
    
    wifiManager.server->on("/callback-check", HTTP_GET, [this]() {
        wifiManager.server->send(200, "text/plain", "Callback endpoint working!");
    });
    Console::printlnR("✓ Registered: /callback-check");
    
    // 2. Custom page
    wifiManager.server->on("/custom", HTTP_GET, [this]() {
        String html = generateScheduleManagementPage();
        wifiManager.server->send(200, "text/html; charset=utf-8", html);
    });
    Console::printlnR("✓ Registered: /custom");
    
    // 3. Close portal endpoint
    wifiManager.server->on("/close", HTTP_GET, [this]() {
        wifiManager.server->send(200, "text/html", "<h1>Portal Closed</h1><p>WiFi portal has been closed.</p>");
        Console::printlnR("Portal close requested via /close endpoint");
        // Note: Actual portal closing logic should be implemented here
    });
    Console::printlnR("✓ Registered: /close");
    
    // 4. Schedule API endpoints (the critical ones)
    Console::printlnR("=== REGISTERING SCHEDULE API ENDPOINTS ===");
    setupScheduleAPIEndpoints();
    
    Console::printlnR("=== ALL ENDPOINTS REGISTRATION COMPLETE ===");
}

/**
 * Scan for available WiFi networks
 */
void WiFiController::scanNetworks() {
    Console::printlnR(F("Scanning WiFi networks..."));
    
    int networkCount = WiFi.scanNetworks();
    
    if (networkCount == 0) {
        Console::printlnR(F("No networks found"));
        return;
    }
    
    Console::printlnR(F(""));
    Console::printlnR(F("=== Available WiFi Networks ==="));
    Console::printR(F("Found "));
    Console::printR(String(networkCount));
    Console::printlnR(F(" networks:"));
    Console::printlnR(F(""));
    
    for (int i = 0; i < networkCount; i++) {
        Console::printR(String(i + 1));
        Console::printR(F(". "));
        
        // Network name
        Console::printR(WiFi.SSID(i));
        
        // Signal strength
        Console::printR(F(" ("));
        Console::printR(String(WiFi.RSSI(i)));
        Console::printR(F(" dBm) "));
        
        // Encryption type
        Console::printR(F("["));
        Console::printR(encryptionTypeStr(WiFi.encryptionType(i)));
        Console::printR(F("]"));
        
        // Check if network is saved
        String savedPassword;
        if (loadNetworkCredentials(WiFi.SSID(i), savedPassword)) {
            Console::printR(F(" *SAVED*"));
        }
        
        Console::printlnR(F(""));
    }
    Console::printlnR(F("=============================="));
}

/**
 * Connect to a WiFi network
 */
bool WiFiController::connectToNetwork(const String& ssid, const String& password, bool saveCredentials) {
    Console::printR(F("Connecting to WiFi: "));
    Console::printlnR(ssid);
    
    if (isConnected && currentSSID == ssid) {
        Console::printlnR(F("Already connected to this network"));
        return true;
    }
    
    // Disconnect from current network if connected
    if (isConnected) {
        WiFi.disconnect();
        connectionState = WIFI_DISCONNECTING;
        connectionStateTime = millis();
        return false; // Connection in progress, will be checked later
    }
    
    lastConnectionAttempt = millis();
    WiFi.begin(ssid.c_str(), password.c_str());
    connectionState = WIFI_CONNECTING;
    connectionStateTime = millis();
    connectionAttempts = 0;
    
    Console::printlnR(F("Starting non-blocking connection..."));
    
    // Store connection parameters for state machine
    pendingSSID = ssid;
    pendingPassword = password;
    pendingSaveCredentials = saveCredentials;
    
    return false; // Connection in progress, will be completed by processConnectionState()
}

/**
 * Connect to a previously saved network
 */
bool WiFiController::connectToSavedNetwork(const String& ssid) {
    String password;
    if (!loadNetworkCredentials(ssid, password)) {
        Console::printR(F("Network '"));
        Console::printR(ssid);
        Console::printlnR(F("' not found in saved networks"));
        return false;
    }
    
    return connectToNetwork(ssid, password, false); // Don't save again
}

/**
 * Disconnect from current WiFi network
 */
void WiFiController::disconnectWiFi() {
    if (isConnected) {
        Console::printR(F("Disconnecting from: "));
        Console::printlnR(currentSSID);
        WiFi.disconnect();
        isConnected = false;
        currentSSID = "";
        Console::printlnR(F("WiFi disconnected"));
    } else {
        Console::printlnR(F("Not connected to any network"));
    }
}

/**
 * List all saved networks
 */
void WiFiController::listSavedNetworks() {
    Console::printlnR(F(""));
    Console::printlnR(F("=== Saved WiFi Networks ==="));
    
    size_t networkCount = preferences.getBytesLength("network_count");
    if (networkCount == 0) {
        Console::printlnR(F("No saved networks"));
        Console::printlnR(F("==========================="));
        return;
    }
    
    // Get the count of saved networks
    uint8_t count = preferences.getUChar("network_count", 0);
    
    if (count == 0) {
        Console::printlnR(F("No saved networks"));
    } else {
        Console::printR(F("Found "));
        Console::printR(String(count));
        Console::printlnR(F(" saved networks:"));
        Console::printlnR(F(""));
        
        for (uint8_t i = 0; i < count; i++) {
            String key = "ssid_" + String(i);
            String ssid = preferences.getString(key.c_str(), "");
            
            if (ssid.length() > 0) {
                Console::printR(String(i + 1));
                Console::printR(F(". "));
                Console::printR(ssid);
                
                // Show if currently connected
                if (isConnected && ssid == currentSSID) {
                    Console::printR(F(" *CONNECTED*"));
                }
                Console::printlnR(F(""));
            }
        }
    }
    Console::printlnR(F("==========================="));
}

/**
 * Remove a saved network
 */
void WiFiController::removeSavedNetwork(const String& ssid) {
    Console::printR(F("Removing saved network: "));
    Console::printlnR(ssid);
    
    removeNetworkCredentials(ssid);
    Console::printlnR(F("Network removed from saved list"));
}

/**
 * Clear all saved networks
 */
void WiFiController::clearAllSavedNetworks() {
    Console::printlnR(F("Clearing all saved networks..."));
    preferences.clear();
    Console::printlnR(F("All saved networks cleared"));
}

/**
 * Check if WiFi is connected
 */
bool WiFiController::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

/**
 * Get current SSID
 */
String WiFiController::getCurrentSSID() {
    if (isWiFiConnected()) {
        return WiFi.SSID();
    }
    return "";
}

/**
 * Get signal strength
 */
int WiFiController::getSignalStrength() {
    if (isWiFiConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

/**
 * Get local IP address
 */
String WiFiController::getLocalIP() {
    if (isWiFiConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

/**
 * Get MAC address
 */
String WiFiController::getMACAddress() {
    return WiFi.macAddress();
}

/**
 * Show current WiFi status
 */
void WiFiController::showWiFiStatus() {
    Console::printlnR(F(""));
    Console::printlnR(F("=== WiFi Status ==="));
    
    if (isWiFiConnected()) {
        Console::printR(F("Status: CONNECTED to "));
        Console::printlnR(getCurrentSSID());
        Console::printR(F("IP Address: "));
        Console::printlnR(getLocalIP());
        Console::printR(F("Signal Strength: "));
        Console::printR(String(getSignalStrength()));
        Console::printlnR(F(" dBm"));
        Console::printR(F("Gateway: "));
        Console::printlnR(WiFi.gatewayIP().toString());
        Console::printR(F("DNS: "));
        Console::printlnR(WiFi.dnsIP().toString());
    } else {
        Console::printlnR(F("Status: DISCONNECTED"));
    }
    
    Console::printR(F("MAC Address: "));
    Console::printlnR(getMACAddress());
    Console::printlnR(F("=================="));
}

/**
 * Process WiFi commands
 */
bool WiFiController::processWiFiCommand(const String& command) {
    if (command == "WIFI SCAN") {
        scanNetworks();
        return true;
    }
    else if (command.startsWith("WIFI CONNECT ")) {
        // Simple parsing: WIFI CONNECT SSID PASSWORD (space-separated)
        int firstSpace = command.indexOf(' ', 13); // After "WIFI CONNECT "
        if (firstSpace > 0) {
            int secondSpace = command.indexOf(' ', firstSpace + 1);
            if (secondSpace > 0) {
                String ssid = command.substring(13, firstSpace);
                String password = command.substring(firstSpace + 1, secondSpace);
                connectToNetwork(ssid, password, true);
            } else {
                // Connect to saved network
                String ssid = command.substring(13);
                ssid.trim();
                connectToSavedNetwork(ssid);
            }
        } else {
            Console::printlnR(F("Usage: WIFI CONNECT SSID PASSWORD"));
        }
        return true;
    }
    else if (command == "WIFI DISCONNECT") {
        disconnectWiFi();
        return true;
    }
    else if (command == "WIFI STATUS") {
        showWiFiStatus();
        return true;
    }
    else if (command == "WIFI LIST") {
        listSavedNetworks();
        return true;
    }
    else if (command.startsWith("WIFI REMOVE ")) {
        String ssid = command.substring(12);
        ssid.trim();
        removeSavedNetwork(ssid);
        return true;
    }
    else if (command == "WIFI CLEAR") {
        clearAllSavedNetworks();
        return true;
    }
    else if (command == "WIFI PORTAL") {
        startConfigPortal();
        return true;
    }
    else if (command.startsWith("WIFI PORTAL ")) {
        String apName = command.substring(12); // Skip "WIFI PORTAL "
        apName.trim();
        startConfigPortal(apName);
        return true;
    }
    else if (command == "WIFI PORTAL STOP") {
        stopConfigPortal();
        return true;
    }
    else if (command == "WIFI TEST") {
        if (testInternetConnection()) {
            Console::printlnR(F("Internet connection: SUCCESS - ESP32 is online!"));
        } else {
            Console::printlnR(F("Internet connection: FAILED - No internet access"));
        }
        return true;
    }
    else if (command == "WIFI DNS CONFIG") {
        configureDNSServers();
        return true;
    }
    else if (command == "WIFI DNS TEST") {
        testDNSServers();
        return true;
    }
    else if (command == "WIFI PORTAL START") {
        if (!configPortalActive) {
            startAlwaysOnPortal();
            Console::printlnR(F("Always-on portal restarted"));
        } else {
            Console::printlnR(F("Portal is already active"));
        }
        return true;
    }
    
    return false;
}

/**
 * Handle auto-reconnection
 */
void WiFiController::handleAutoReconnect() {
    if (!isWiFiConnected() && currentSSID.length() > 0) {
        // Try to reconnect every 30 seconds
        if (millis() - lastConnectionAttempt > WIFI_RECONNECT_INTERVAL) {
            Console::printlnR(F("Attempting WiFi auto-reconnection..."));
            String password;
            if (loadNetworkCredentials(currentSSID, password)) {
                connectToNetwork(currentSSID, password, false);
            }
        }
    }
}

/**
 * Check connection status and handle disconnections
 */
void WiFiController::checkConnectionStatus() {
    // Check connection status periodically
    if (millis() - lastConnectionCheck > WIFI_CONNECTION_CHECK_INTERVAL) {
        bool currentlyConnected = isWiFiConnected();
        
        // Connection lost - start portal if configured
        if (wasConnectedBefore && !currentlyConnected && !configPortalActive) {
            Console::printlnR(F("WiFi connection lost!"));
            isConnected = false;
            
            if (WIFI_PORTAL_ON_DISCONNECT) {
                startPortalOnDisconnect();
            }
        }
        
        // Update connection state
        isConnected = currentlyConnected;
        if (isConnected) {
            currentSSID = WiFi.SSID();
            wasConnectedBefore = true;
        }
        
        lastConnectionCheck = millis();
    }
    
    // Process non-blocking connection state machine
    processConnectionState();
}

/**
 * Process non-blocking WiFi connection state machine
 * This method should be called regularly (e.g., every 500ms) to handle connection state
 */
void WiFiController::processConnectionState() {
    switch (connectionState) {
        case WIFI_IDLE:
            // No connection in progress
            break;
            
        case WIFI_DISCONNECTING:
            // Wait 1 second after disconnect before connecting
            if (millis() - connectionStateTime >= 1000) {
                WiFi.begin(pendingSSID.c_str(), pendingPassword.c_str());
                connectionState = WIFI_CONNECTING;
                connectionStateTime = millis();
                connectionAttempts = 0;
            }
            break;
            
        case WIFI_CONNECTING:
            // Check connection status every 500ms
            if (millis() - connectionStateTime >= CONNECTION_CHECK_INTERVAL) {
                connectionAttempts++;
                
                if (WiFi.status() == WL_CONNECTED) {
                    // Connection successful
                    isConnected = true;
                    wasConnectedBefore = true;
                    currentSSID = pendingSSID;
                    
                    if (pendingSaveCredentials) {
                        saveNetworkCredentials(pendingSSID, pendingPassword);
                    }
                    
                    Console::printlnR(F("✓ WiFi connected successfully!"));
                    printNetworkDetails();
                    configureDNSServers();
                    
                    connectionState = WIFI_CONNECTED;
                } else if (connectionAttempts >= MAX_CONNECTION_ATTEMPTS) {
                    // Connection failed after max attempts
                    isConnected = false;
                    currentSSID = "";
                    Console::printlnR(F("✗ Failed to connect to WiFi"));
                    Console::printR(F("Status: "));
                    Console::printlnR(String(WiFi.status()));
                    
                    connectionState = WIFI_FAILED;
                } else {
                    // Still trying to connect
                    Console::printR(F("."));
                    connectionStateTime = millis();
                }
            }
            break;
            
        case WIFI_CONNECTED:
        case WIFI_FAILED:
            // Reset to idle state after completion
            connectionState = WIFI_IDLE;
            break;
    }
}

/**
 * Start portal automatically on boot
 */
void WiFiController::startPortalOnBoot() {
    Console::printlnR(F("Auto-starting WiFi configuration portal on boot..."));
    Console::printR(F("Portal will be available for "));
    Console::printR(String(WIFI_PORTAL_TIMEOUT / 60000));
    Console::printlnR(F(" minutes"));
    
    startConfigPortal(WIFI_PORTAL_AP_NAME);
}

/**
 * Start portal when connection is lost
 */
void WiFiController::startPortalOnDisconnect() {
    Console::printlnR(F("Starting WiFi portal due to connection loss..."));
    Console::printlnR(F("Connect to configure a new network"));
    
    startConfigPortal(WIFI_PORTAL_AP_NAME);
}

/**
 * Try to auto-connect to saved networks on boot
 */
bool WiFiController::tryAutoConnect() {
    Console::printlnR(F("Attempting auto-connection..."));
    
    // First, try WiFiManager's autoConnect (uses saved credentials from portal)
    Console::printlnR(F("Trying WiFiManager saved credentials..."));
    wifiManager.setConfigPortalTimeout(10); // Short timeout for auto-connect
    
    // Try to connect without starting portal
    if (wifiManager.autoConnect()) {
        isConnected = true;
        wasConnectedBefore = true;
        currentSSID = WiFi.SSID();
        
        Console::printlnR(F("✓ WiFiManager auto-connection successful!"));
        Console::printR(F("Connected to: "));
        Console::printlnR(currentSSID);
        printNetworkDetails();
        
        // Restore original timeout
        wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT / 1000);
        return true;
    }
    
    // Restore original timeout
    wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT / 1000);
    
    // Fallback: Try our custom saved networks
    Console::printlnR(F("WiFiManager auto-connect failed, trying custom saved networks..."));
    
    uint8_t count = preferences.getUChar("network_count", 0);
    
    if (count == 0) {
        Console::printlnR(F("No custom saved networks found"));
        return false;
    }
    
    Console::printR(F("Found "));
    Console::printR(String(count));
    Console::printlnR(F(" custom saved networks, trying to connect..."));
    
    // Try each saved network in order
    for (uint8_t i = 0; i < count; i++) {
        String ssidKey = "ssid_" + String(i);
        String passKey = "pass_" + String(i);
        
        String savedSSID = preferences.getString(ssidKey.c_str(), "");
        String savedPassword = preferences.getString(passKey.c_str(), "");
        
        if (savedSSID.length() > 0) {
            Console::printR(F("Trying network: "));
            Console::printlnR(savedSSID);
            
            // Attempt connection with timeout
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
            
            Console::printR(F("Connecting"));
            int attempts = 0;
            const int maxAttempts = 20; // 10 seconds timeout
            
            while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
                delay(500);
                Console::printR(F("."));
                attempts++;
            }
            Console::printlnR(F(""));
            
            if (WiFi.status() == WL_CONNECTED) {
                isConnected = true;
                wasConnectedBefore = true;
                currentSSID = savedSSID;
                
                Console::printlnR(F("✓ Custom network auto-connection successful!"));
                Console::printR(F("Connected to: "));
                Console::printlnR(savedSSID);
                printNetworkDetails();
                return true;
            } else {
                Console::printR(F("✗ Failed to connect to "));
                Console::printlnR(savedSSID);
            }
        }
    }
    
    Console::printlnR(F("Could not connect to any saved network"));
    return false;
}

/**
 * Start tzapu WiFiManager configuration portal (Always-On)
 */
void WiFiController::startConfigPortal(const String& apName) {
    if (configPortalActive) {
        Console::printlnR(F("Always-on configuration portal is already active"));
        Console::printlnR(F("Connect to WiFi AP and visit http://192.168.4.1"));
        return;
    }
    
    Console::printlnR(F("Starting always-on WiFi Configuration Portal..."));
    startAlwaysOnPortal();
}

/**
 * Stop configuration portal
 */
void WiFiController::stopConfigPortal() {
    if (!configPortalActive && !portalStartRequested) {
        Console::printlnR(F("Configuration portal is not active"));
        return;
    }
    
    Console::printlnR(F("Stopping configuration portal..."));
    
    if (configPortalActive) {
        // Stop WiFiManager web portal
        wifiManager.stopConfigPortal();
        
        // Stop the Access Point
        WiFi.softAPdisconnect(true);
        
        // Switch back to STA mode only if connected to WiFi
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.mode(WIFI_STA);
            Console::printlnR(F("✓ Switched to Station mode - WiFi connection maintained"));
        } else {
            WiFi.mode(WIFI_OFF);
            Console::printlnR(F("✓ WiFi turned off - no connections active"));
        }
        
        configPortalActive = false;
    }
    
    if (portalStartRequested) {
        portalStartRequested = false;
    }
    
    Console::printlnR(F("✓ Access Point and configuration portal stopped"));
    Console::printlnR(F("✓ Portal is no longer accessible via http://192.168.4.1"));
}

/**
 * Check if configuration portal is active
 */
bool WiFiController::isConfigPortalActive() {
    return configPortalActive || portalStartRequested;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

/**
 * Print network connection details
 */
void WiFiController::printNetworkDetails() {
    Console::printR(F("IP Address: "));
    Console::printlnR(WiFi.localIP().toString());
    Console::printR(F("Signal Strength: "));
    Console::printR(String(WiFi.RSSI()));
    Console::printlnR(F(" dBm"));
}

/**
 * Convert encryption type to string
 */
String WiFiController::encryptionTypeStr(wifi_auth_mode_t encryptionType) {
    switch (encryptionType) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "UNKNOWN";
    }
}

/**
 * Save network credentials to preferences
 */
void WiFiController::saveNetworkCredentials(const String& ssid, const String& password) {
    // Get current count
    uint8_t count = preferences.getUChar("network_count", 0);
    
    // Check if network already exists
    for (uint8_t i = 0; i < count; i++) {
        String key = "ssid_" + String(i);
        String savedSSID = preferences.getString(key.c_str(), "");
        if (savedSSID == ssid) {
            // Update existing entry
            String passKey = "pass_" + String(i);
            preferences.putString(passKey.c_str(), password);
            Console::printlnR(F("Network credentials updated"));
            return;
        }
    }
    
    // Add new entry
    String ssidKey = "ssid_" + String(count);
    String passKey = "pass_" + String(count);
    
    preferences.putString(ssidKey.c_str(), ssid);
    preferences.putString(passKey.c_str(), password);
    preferences.putUChar("network_count", count + 1);
    
    Console::printlnR(F("Network credentials saved"));
}

/**
 * Load network credentials from preferences
 */
bool WiFiController::loadNetworkCredentials(const String& ssid, String& password) {
    uint8_t count = preferences.getUChar("network_count", 0);
    
    for (uint8_t i = 0; i < count; i++) {
        String key = "ssid_" + String(i);
        String savedSSID = preferences.getString(key.c_str(), "");
        if (savedSSID == ssid) {
            String passKey = "pass_" + String(i);
            password = preferences.getString(passKey.c_str(), "");
            return true;
        }
    }
    return false;
}

/**
 * Remove network credentials from preferences
 */
void WiFiController::removeNetworkCredentials(const String& ssid) {
    uint8_t count = preferences.getUChar("network_count", 0);
    
    for (uint8_t i = 0; i < count; i++) {
        String key = "ssid_" + String(i);
        String savedSSID = preferences.getString(key.c_str(), "");
        if (savedSSID == ssid) {
            // Remove this entry by shifting all subsequent entries
            for (uint8_t j = i; j < count - 1; j++) {
                String srcSSIDKey = "ssid_" + String(j + 1);
                String srcPassKey = "pass_" + String(j + 1);
                String dstSSIDKey = "ssid_" + String(j);
                String dstPassKey = "pass_" + String(j);
                
                String tempSSID = preferences.getString(srcSSIDKey.c_str(), "");
                String tempPass = preferences.getString(srcPassKey.c_str(), "");
                
                preferences.putString(dstSSIDKey.c_str(), tempSSID);
                preferences.putString(dstPassKey.c_str(), tempPass);
            }
            
            // Remove the last entry
            String lastSSIDKey = "ssid_" + String(count - 1);
            String lastPassKey = "pass_" + String(count - 1);
            preferences.remove(lastSSIDKey.c_str());
            preferences.remove(lastPassKey.c_str());
            
            // Update count
            preferences.putUChar("network_count", count - 1);
            return;
        }
    }
}

/**
 * Test internet connectivity by making HTTP request to Google
 * Non-blocking implementation with timeout
 */
bool WiFiController::testInternetConnection() {
    // Check if WiFi is connected first
    if (!isWiFiConnected()) {
        Console::printlnR(F("WiFi not connected - cannot test internet"));
        return false;
    }
    
    Console::printlnR(F("Testing internet connectivity..."));
    Console::printlnR(F("Making HTTP request to google.com"));
    
    WiFiClient client;
    const char* host = "google.com";
    const int httpPort = 80;
    const int timeout = 5000; // 5 second timeout
    
    // Set timeout for connection
    client.setTimeout(timeout);
    
    // Try to connect to Google
    if (!client.connect(host, httpPort)) {
        Console::printlnR(F("Connection to google.com failed"));
        return false;
    }
    
    // Send HTTP GET request
    client.print(String("GET / HTTP/1.1\r\n") +
                "Host: " + host + "\r\n" + 
                "Connection: close\r\n\r\n");
    
    // Wait for response with timeout
    unsigned long startTime = millis();
    while (client.connected() && !client.available()) {
        if (millis() - startTime > timeout) {
            Console::printlnR(F("Request timeout"));
            client.stop();
            return false;
        }
        yield(); // ESP32 yield to prevent watchdog timeout
    }
    
    // Check if we got a response
    bool success = false;
    if (client.available()) {
        String response = client.readStringUntil('\n');
        Console::printR(F("Response: "));
        Console::printlnR(response);
        
        // Check for HTTP 200 OK or any valid HTTP response
        if (response.indexOf("HTTP/1.1") >= 0 || response.indexOf("HTTP/1.0") >= 0) {
            success = true;
        }
    }
    
    client.stop();
    return success;
}

/**
 * Start always-on WiFi portal using tzapu WiFiManager features
 * Uses startConfigPortal with timeout 0 for persistent operation
 */
void WiFiController::startAlwaysOnPortal() {
    Console::printlnR(F("Starting always-on WiFi portal..."));
    
    // Configure tzapu WiFiManager for always-on operation
    wifiManager.setConfigPortalTimeout(0); // Never timeout - CRITICAL
    wifiManager.setConnectRetries(3); // Retry connection attempts
    wifiManager.setBreakAfterConfig(false); // Don't break after successful config
    wifiManager.setDebugOutput(true); // Enable debug to see what's happening
    
    // Configure menu with custom Fish Feeder configuration page
    std::vector<const char*> menu = {"wifi", "info", "custom", "close", "sep", "erase", "restart"};
    wifiManager.setMenu(menu);
    
    // Add custom styling for the portal
    wifiManager.setCustomHeadElement("<style>body{font-family:Arial,sans-serif;}</style>");
    wifiManager.setTitle("🔌 Fish Feeder WiFi Setup");
    
    // Set custom menu HTML for the "custom" menu item
    const char* customMenuHTML = "<form action='/custom' method='get'><button>Configure Fish Feeder</button></form><br/>\n";
    wifiManager.setCustomMenuHTML(customMenuHTML);
    
    // Set AP credentials
    String apName = WIFI_PORTAL_AP_NAME;
    const char* apPassword = strlen(WIFI_PORTAL_AP_PASSWORD) > 0 ? WIFI_PORTAL_AP_PASSWORD : nullptr;
    
    Console::printR(F("Portal AP Name: "));
    Console::printlnR(apName);
    if (apPassword) {
        Console::printR(F("Portal AP Password: "));
        Console::printlnR(apPassword);
    } else {
        Console::printlnR(F("Portal AP: Open (no password)"));
    }
    Console::printlnR(F("Portal URL: http://192.168.4.1"));
    
    // Use direct AP setup + manual web server approach for guaranteed always-on
    configPortalActive = true;
    
    // Setup Access Point manually first
    WiFi.mode(WIFI_AP_STA);
    bool apStarted;
    
    if (apPassword) {
        apStarted = WiFi.softAP(apName.c_str(), apPassword);
    } else {
        apStarted = WiFi.softAP(apName.c_str());
    }
    
    if (apStarted) {
        Console::printR(F("✓ Access Point started: "));
        Console::printlnR(WiFi.softAPIP().toString());
        
        // Configure web server callback for custom endpoints
        Console::printlnR("=== CONFIGURING WEB SERVER CALLBACK ===");
        wifiManager.setWebServerCallback([this]() {
            Console::printlnR("=== WEB SERVER CALLBACK ACTIVATED ===");
            Console::printlnR("Server available - callback executed at: " + String(millis()) + "ms");
            Console::printlnR("NOTE: Endpoints will be registered later via registerAllEndpoints()");
            Console::printlnR("=== WEB SERVER CALLBACK SETUP COMPLETE ===");
        });
        
        // Now let WiFiManager handle the web portal on the existing AP
        // This ensures the web server runs on our persistent AP
        wifiManager.startWebPortal();
        
        // Register endpoints directly after starting portal as backup
        Console::printlnR("=== REGISTERING ENDPOINTS DIRECTLY ===");
        if (wifiManager.server) {
            wifiManager.server->on("/api/test", HTTP_GET, [this]() {
                Console::printlnR("=== DIRECT API TEST ENDPOINT CALLED ===");
                wifiManager.server->send(200, "application/json", "{\"status\":\"Direct API working\"}");
            });
            
            wifiManager.server->on("/custom", HTTP_GET, [this]() {
                Console::printlnR("=== DIRECT CUSTOM PAGE REQUEST ===");
                String html = generateScheduleManagementPage();
                wifiManager.server->send(200, "text/html; charset=utf-8", html);
            });
            
            // NOTE: Schedule API endpoints will be registered later via registerAllEndpoints()
            // after ModuleManager is configured in main.cpp
            Console::printlnR("=== BASIC ENDPOINTS REGISTERED ===");
            Console::printlnR("NOTE: API endpoints will be registered after ModuleManager setup");
        } else {
            Console::printlnR("=== ERROR: WiFiManager server is NULL ===");
        }
        
        Console::printlnR(F("✓ Always-on WiFi portal web server is active!"));
        Console::printlnR(F("✓ Portal accessible at http://192.168.4.1"));
        Console::printlnR(F("✓ Custom 'Close AP' button available in portal"));
        Console::printlnR(F("✓ Portal will remain active until manually closed"));
    } else {
        Console::printlnR(F("⚠ Failed to start Access Point"));
        configPortalActive = false;
    }
}

/**
 * Configure DNS servers with priority order
 */
void WiFiController::configureDNSServers() {
    if (!isWiFiConnected()) {
        Console::printlnR(F("Cannot configure DNS: WiFi not connected"));
        return;
    }
    
    Console::printlnR(F("Configuring DNS servers..."));
    Console::printR(F("Available DNS servers ("));
    Console::printR(String(DNS_SERVERS_COUNT));
    Console::printlnR(F("):"));
    
    for (int i = 0; i < DNS_SERVERS_COUNT; i++) {
        Console::printR(F("  "));
        Console::printR(String(i + 1));
        Console::printR(F(". "));
        Console::printR(DNS_SERVERS[i]);
        if (i == 0) {
            Console::printlnR(F(" (primary)"));
        } else {
            Console::printlnR(F(" (fallback)"));
        }
    }
    
    // Configure primary and secondary DNS
    IPAddress primaryDNS, secondaryDNS;
    
    if (DNS_SERVERS_COUNT >= 1) {
        primaryDNS.fromString(DNS_SERVERS[0]);
    }
    if (DNS_SERVERS_COUNT >= 2) {
        secondaryDNS.fromString(DNS_SERVERS[1]);
    }
    
    // Apply DNS configuration
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS);
    
    Console::printR(F("✓ Primary DNS: "));
    Console::printlnR(primaryDNS.toString());
    Console::printR(F("✓ Secondary DNS: "));
    Console::printlnR(secondaryDNS.toString());
    Console::printlnR(F("DNS configuration completed"));
}

/**
 * Test all DNS servers for connectivity
 */
void WiFiController::testDNSServers() {
    if (!isWiFiConnected()) {
        Console::printlnR(F("Cannot test DNS: WiFi not connected"));
        return;
    }
    
    Console::printlnR(F("Testing DNS servers..."));
    const char* testDomain = "google.com";
    
    for (int i = 0; i < DNS_SERVERS_COUNT; i++) {
        Console::printR(F("Testing DNS "));
        Console::printR(DNS_SERVERS[i]);
        Console::printR(F(" with "));
        Console::printR(testDomain);
        Console::printR(F("... "));
        
        // Configure this DNS server temporarily
        IPAddress dnsIP;
        dnsIP.fromString(DNS_SERVERS[i]);
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dnsIP);
        
        delay(100); // Small delay for DNS to take effect
        
        // Test resolution
        IPAddress resolvedIP;
        if (WiFi.hostByName(testDomain, resolvedIP)) {
            Console::printR(F("✓ PASS ("));
            Console::printR(resolvedIP.toString());
            Console::printlnR(F(")"));
        } else {
            Console::printlnR(F("✗ FAIL"));
        }
    }
    
    // Restore primary DNS configuration
    configureDNSServers();
    Console::printlnR(F("DNS test completed"));
}

/**
 * Process WiFi configuration portal in non-blocking mode
 * Call this regularly from main loop or task
 */
void WiFiController::processConfigPortal() {
    // Check if AP shutdown was requested via web portal
    if (shutdownRequested) {
        Console::printlnR(F("Shutting down Access Point as requested via web portal..."));
        shutdownRequested = false;
        stopConfigPortal();
        return;
    }
    
    // Always-on portal: just process WiFiManager requests
    if (configPortalActive) {
        // Process portal requests - this handles both AP and STA connections
        wifiManager.process();
        
        // Check for new WiFi connection
        if (WiFi.status() == WL_CONNECTED && currentSSID != WiFi.SSID()) {
            // New connection established
            isConnected = true;
            wasConnectedBefore = true;
            currentSSID = WiFi.SSID();
            
            Console::printlnR(F("✓ New WiFi connection established via portal!"));
            Console::printlnR(F("✓ Credentials saved by WiFiManager for auto-reconnection"));
            Console::printlnR(F("✓ Portal remains active for future configuration"));
            printNetworkDetails();
            configureDNSServers();
        }
        
        // Check for connection loss
        if (isConnected && WiFi.status() != WL_CONNECTED) {
            Console::printlnR(F("WiFi connection lost - portal remains active"));
            isConnected = false;
            currentSSID = "";
        }
    }
    
    // Handle manual portal start requests (if any)
    if (portalStartRequested && !configPortalActive) {
        Console::printlnR(F("Manual portal start requested - activating always-on portal"));
        startAlwaysOnPortal();
        portalStartRequested = false;
    }
}

/**
 * Generate complete Feeding Schedule Management Interface
 */
String WiFiController::generateScheduleManagementPage() {
    Console::printlnR("=== GENERATING SCHEDULE MANAGEMENT PAGE ===");
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'>";
    html += "<meta http-equiv='Pragma' content='no-cache'>";
    html += "<meta http-equiv='Expires' content='0'>";
    html += "<title>&#128031; Fish Feeder - Schedule Management</title>";
    html += "<style>";
    
    // Modern CSS styling
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }";
    html += ".container { max-width: 1200px; margin: 0 auto; background: rgba(255,255,255,0.95); border-radius: 20px; box-shadow: 0 20px 40px rgba(0,0,0,0.1); overflow: hidden; }";
    html += ".header { background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%); color: white; padding: 30px; text-align: center; }";
    html += ".header h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }";
    html += ".header p { font-size: 1.2em; opacity: 0.9; }";
    
    // Status cards at the top
    html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; padding: 30px; background: #f8f9fc; }";
    html += ".status-card { background: white; border-radius: 15px; padding: 25px; box-shadow: 0 5px 15px rgba(0,0,0,0.08); border-left: 5px solid #3498db; }";
    html += ".status-card.last { border-left-color: #e74c3c; }";
    html += ".status-card.next { border-left-color: #27ae60; }";
    html += ".status-card h3 { color: #2c3e50; margin-bottom: 15px; font-size: 1.1em; }";
    html += ".status-value { font-size: 1.4em; font-weight: 600; color: #34495e; }";
    html += ".status-disabled { color: #757575 !important; }";
    html += ".status-time { color: #7f8c8d; font-size: 0.9em; margin-top: 5px; }";
    
    // Schedule management section
    html += ".content { padding: 30px; }";
    html += ".section { margin-bottom: 40px; }";
    html += ".section h2 { color: #2c3e50; margin-bottom: 20px; font-size: 1.8em; border-bottom: 3px solid #3498db; padding-bottom: 10px; }";
    
    // Schedule table styling
    html += ".schedule-table { width: 100%; border-collapse: collapse; background: white; border-radius: 10px; overflow: hidden; box-shadow: 0 5px 15px rgba(0,0,0,0.08); }";
    html += ".schedule-table th { background: #34495e; color: white; padding: 15px; text-align: left; font-weight: 600; }";
    html += ".schedule-table td { padding: 10px; border-bottom: 1px solid #ecf0f1; vertical-align: middle; }";
    html += ".schedule-table tr:hover { background: #f8f9fc; }";
    html += ".schedule-table tr:last-child td { border-bottom: none; }";
    html += ".schedule-table input[type='number'], .schedule-table input[type='text'], .schedule-table select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }";
    html += ".schedule-table input[type='number']:focus, .schedule-table input[type='text']:focus, .schedule-table select:focus { outline: none; border-color: #3498db; }";
    html += ".schedule-table input[type='checkbox'] { width: 20px; height: 20px; cursor: pointer; }";
    html += ".schedule-table .time-inputs { display: flex; gap: 5px; align-items: center; }";
    html += ".schedule-table .time-inputs input { width: 50px; text-align: center; }";
    html += ".schedule-table .time-inputs span { color: #7f8c8d; font-weight: bold; }";
    
    // Buttons and forms
    html += ".btn { display: inline-block; padding: 12px 24px; margin: 5px; border: none; border-radius: 8px; cursor: pointer; font-size: 14px; font-weight: 600; text-decoration: none; transition: all 0.3s ease; }";
    html += ".btn:disabled { opacity: 0.5; cursor: not-allowed; }";
    html += ".btn-primary { background: #3498db; color: white; }";
    html += ".btn-primary:hover:not(:disabled) { background: #2980b9; transform: translateY(-2px); }";
    html += ".btn-success { background: #27ae60; color: white; }";
    html += ".btn-success:hover:not(:disabled) { background: #229954; transform: translateY(-2px); }";
    html += ".btn-danger { background: #e74c3c; color: white; }";
    html += ".btn-danger:hover:not(:disabled) { background: #c0392b; transform: translateY(-2px); }";
    html += ".btn-warning { background: #f39c12; color: white; }";
    html += ".btn-warning:hover:not(:disabled) { background: #e67e22; transform: translateY(-2px); }";
    html += ".btn-small { padding: 8px 16px; font-size: 12px; }";
    html += ".btn-large { padding: 15px 40px; font-size: 16px; margin-top: 20px; }";
    html += ".btn-block { display: block; width: 100%; text-align: center; }";
    
    // Forms
    html += ".form-group { margin-bottom: 20px; }";
    html += ".form-group label { display: block; margin-bottom: 8px; color: #2c3e50; font-weight: 600; }";
    html += ".form-control { width: 100%; padding: 12px; border: 2px solid #ecf0f1; border-radius: 8px; font-size: 14px; transition: border-color 0.3s ease; }";
    html += ".form-control:focus { outline: none; border-color: #3498db; }";
    html += ".form-row { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; }";
    
    // Add/Edit form
    html += ".add-form { background: #f8f9fc; padding: 25px; border-radius: 15px; margin-bottom: 30px; border: 2px dashed #bdc3c7; }";
    html += ".add-form h3 { color: #2c3e50; margin-bottom: 20px; }";
    
    // Status indicators
    html += ".status-enabled { color: #27ae60; font-weight: 600; }";
    html += ".status-disabled { color: #e74c3c; font-weight: 600; }";
    html += ".back-link { display: inline-block; margin-top: 30px; padding: 15px 30px; background: #95a5a6; color: white; border-radius: 8px; text-decoration: none; font-weight: 600; }";
    html += ".back-link:hover { background: #7f8c8d; }";
    
    // Responsive design
    html += "@media (max-width: 768px) {";
    html += ".status-grid { grid-template-columns: 1fr; }";
    html += ".form-row { grid-template-columns: 1fr; }";
    html += ".schedule-table { font-size: 14px; }";
    html += ".schedule-table th, .schedule-table td { padding: 10px; }";
    html += "}";
    
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    
    // Header with real-time clock
    html += "<div class='header'>";
    html += "<h1>&#128031; Smart Fish Feeder</h1>";
    html += "<p>Automated Feeding Schedule Management</p>";
    html += "<div id='realtime-clock' style='font-size: 1.1em; margin-top: 15px; padding: 10px; background: rgba(255,255,255,0.1); border-radius: 8px; font-family: monospace;'>";
    html += "<span id='current-datetime'>Loading...</span>";
    html += "</div>";
    html += "</div>";
    
    // Status cards at the top
    html += "<div class='status-grid'>";
    
    // WiFi Status Card
    html += "<div class='status-card'>";
    html += "<h3>&#128246; WiFi Status</h3>";
    if (isConnected) {
        html += "<div class='status-value status-enabled'>&#10003; Connected</div>";
        html += "<div class='status-time'>Network: " + currentSSID + "</div>";
    } else {
        html += "<div class='status-value status-disabled'>&#9888; Disconnected</div>";
        html += "<div class='status-time'>Configure connection</div>";
    }
    html += "</div>";
    
    // Last Feeding Card
    html += "<div class='status-card last'>";
    html += "<h3>&#128337; Last Feeding</h3>";
    html += "<div class='status-value' id='lastFeedingValue'>Loading...</div>";
    html += "<div class='status-time'>From schedule system</div>";
    html += "</div>";
    
    // Next Feeding Card
    html += "<div class='status-card next'>";
    html += "<h3>&#128336; Next Feeding</h3>";
    html += "<div class='status-value' id='nextFeedingValue'>Loading...</div>";
    html += "<div class='status-time'>Automatic schedule</div>";
    html += "</div>";
    
    html += "</div>"; // End status-grid
    
    // Main content
    html += "<div class='content'>";
    
    // Quick Actions Section
    html += "<div class='section'>";
    html += "<h2>&#9889; Quick Actions</h2>";
    html += "<div style='display: flex; align-items: center; gap: 15px; margin-bottom: 20px; flex-wrap: wrap;'>";
    html += "<label for='portionSelect' style='font-weight: 600; color: #2c3e50;'>Portions:</label>";
    html += "<select id='portionSelect' class='form-control' style='width: 120px;'>";
    for (int i = 1; i <= 20; i++) {
        html += "<option value='" + String(i) + "'";
        if (i == 2) html += " selected"; // Default to 2 portions
        html += ">" + String(i) + "</option>";
    }
    html += "</select>";
    html += "<button class='btn btn-primary' onclick='feedNowFromSelect()'>&#127860; Feed Now</button>";
    html += "</div>";
    html += "<button class='btn btn-warning' onclick='toggleScheduleSystem()'>Enable/Disable Schedule</button>";
    html += "<button class='btn btn-success' onclick='refreshData()'>Refresh Data</button>";
    html += "</div>";
    
    // Current Schedules Section with inline editing
    html += "<div class='section'>";
    html += "<h2>&#128337; Feeding Schedules</h2>";
    html += "<p style='color: #7f8c8d; margin-bottom: 20px;'>Click on any field to edit. Changes will only take effect after clicking 'Save Changes'.</p>";
    html += "<table class='schedule-table'>";
    html += "<thead>";
    html += "<tr>";
    html += "<th style='width: 150px;'>Time (HH:MM)</th>";
    html += "<th style='width: 100px;'>Portions</th>";
    html += "<th>Description</th>";
    html += "<th style='width: 100px; text-align: center;'>Enabled</th>";
    html += "<th style='width: 80px; text-align: center;'>Delete</th>";
    html += "</tr>";
    html += "</thead>";
    html += "<tbody id='scheduleTable'>";
    html += "<tr>";
    html += "<td colspan='5' style='text-align: center; color: #7f8c8d;'>Loading schedules...</td>";
    html += "</tr>";
    html += "</tbody>";
    html += "</table>";
    
    html += "<div style='margin-top: 20px; display: flex; gap: 15px; flex-wrap: wrap;'>";
    html += "<button class='btn btn-success' onclick='addNewScheduleRow()'>&#10133; Add New Schedule</button>";
    html += "<button id='saveBtn' class='btn btn-primary btn-large' onclick='saveAllChanges()' disabled>&#128190; Save Changes</button>";
    html += "</div>";
    html += "</div>";
    
    // System Configuration
    html += "<div class='section'>";
    html += "<h2>&#9881; System Configuration</h2>";
    html += "<div class='form-row'>";
    html += "<div class='form-group'>";
    html += "<label for='tolerance'>Missed Feeding Tolerance (minutes)</label>";
    html += "<input type='number' id='tolerance' class='form-control' min='1' max='120' value='30'>";
    html += "<button class='btn btn-primary btn-small' onclick='setTolerance()'>Update</button>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='recovery'>Recovery Period (hours)</label>";
    html += "<input type='number' id='recovery' class='form-control' min='1' max='72' value='12'>";
    html += "<button class='btn btn-primary btn-small' onclick='setRecovery()'>Update</button>";
    html += "</div>";
    html += "</div>";
    html += "</div>";
    
    html += "</div>"; // End content
    
    // Back link
    html += "<div style='padding: 20px; text-align: center;'>";
    html += "<a href='/' class='back-link'>&#8592; Back to WiFi Portal</a>";
    html += "</div>";
    
    html += "</div>"; // End container
    
    // JavaScript for inline editing with change tracking
    html += "<script>";
    
    // Global state management
    html += "let originalSchedules = [];";
    html += "let currentSchedules = [];";
    html += "let hasUnsavedChanges = false;";
    html += "let schedulesToDelete = [];";
    html += "let newScheduleCounter = 0;";
    
    // API helper functions
    html += "function apiGet(url, callback) {";
    html += "  const xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', url, true);";
    html += "  xhr.onreadystatechange = function() {";
    html += "    if(xhr.readyState === 4) {";
    html += "      if(xhr.status === 200) {";
    html += "        try { callback(JSON.parse(xhr.responseText)); }";
    html += "        catch(e) { callback(null); }";
    html += "      } else {";
    html += "        callback(null);";
    html += "      }";
    html += "    }";
    html += "  };";
    html += "  xhr.send();";
    html += "}";
    
    // Mark as changed
    html += "function markChanged() {";
    html += "  hasUnsavedChanges = true;";
    html += "  document.getElementById('saveBtn').disabled = false;";
    html += "  document.getElementById('saveBtn').style.background = '#27ae60';";
    html += "}";
    
    // Reset changes
    html += "function resetChanges() {";
    html += "  hasUnsavedChanges = false;";
    html += "  schedulesToDelete = [];";
    html += "  document.getElementById('saveBtn').disabled = true;";
    html += "  document.getElementById('saveBtn').style.background = '';";
    html += "}";
    
    // Load schedules from API
    html += "function loadSchedules() {";
    html += "  apiGet('/api/schedules', function(schedules) {";
    html += "    if(!schedules) {";
    html += "      document.getElementById('scheduleTable').innerHTML = '<tr><td colspan=\\'5\\' style=\\'text-align: center; color: #e74c3c;\\'>Error loading schedules</td></tr>';";
    html += "      return;";
    html += "    }";
    html += "    originalSchedules = JSON.parse(JSON.stringify(schedules));";
    html += "    currentSchedules = JSON.parse(JSON.stringify(schedules));";
    html += "    renderScheduleTable();";
    html += "    resetChanges();";
    html += "  });";
    html += "}";
    
    // Render schedule table with inline editing
    html += "function renderScheduleTable() {";
    html += "  const tableBody = document.getElementById('scheduleTable');";
    html += "  if(currentSchedules.length === 0) {";
    html += "    tableBody.innerHTML = '<tr><td colspan=\\'5\\' style=\\'text-align: center; color: #7f8c8d;\\'>No schedules configured. Click \\'Add New Schedule\\' to create one.</td></tr>';";
    html += "    return;";
    html += "  }";
    html += "  tableBody.innerHTML = '';";
    html += "  currentSchedules.forEach(function(schedule, index) {";
    html += "    if(schedule.markedForDeletion) return;";
    html += "    const row = document.createElement('tr');";
    html += "    if(schedule.isNew) row.style.background = '#e8f5e9';";
    html += "    const hourVal = String(schedule.hour || 0).padStart(2, '0');";
    html += "    const minVal = String(schedule.minute || 0).padStart(2, '0');";
    html += "    let portionsSelect = '<select onchange=\\'updateSchedule(' + index + ', \"portions\", this.value)\\'>';";
    html += "    for(let i = 1; i <= 20; i++) {";
    html += "      portionsSelect += '<option value=\\'' + i + '\\'' + (i === (schedule.portions || 1) ? ' selected' : '') + '>' + i + '</option>';";
    html += "    }";
    html += "    portionsSelect += '</select>';";
    html += "    row.innerHTML = ";
    html += "      '<td><div class=\\'time-inputs\\'>' +";
    html += "        '<input type=\\'number\\' min=\\'0\\' max=\\'23\\' value=\\'' + hourVal + '\\' onchange=\\'updateSchedule(' + index + ', \"hour\", this.value)\\' style=\\'width: 50px;\\' />' +";
    html += "        '<span>:</span>' +";
    html += "        '<input type=\\'number\\' min=\\'0\\' max=\\'59\\' value=\\'' + minVal + '\\' onchange=\\'updateSchedule(' + index + ', \"minute\", this.value)\\' style=\\'width: 50px;\\' />' +";
    html += "      '</div></td>' +";
    html += "      '<td>' + portionsSelect + '</td>' +";
    html += "      '<td><input type=\\'text\\' value=\\'' + (schedule.description || '') + '\\' onchange=\\'updateSchedule(' + index + ', \"description\", this.value)\\' placeholder=\\'Enter description\\' /></td>' +";
    html += "      '<td style=\\'text-align: center;\\'><input type=\\'checkbox\\' ' + (schedule.enabled ? 'checked' : '') + ' onchange=\\'updateSchedule(' + index + ', \"enabled\", this.checked)\\' /></td>' +";
    html += "      '<td style=\\'text-align: center;\\'><button class=\\'btn btn-danger btn-small\\' onclick=\\'markForDeletion(' + index + ')\\'>&#128465;</button></td>';";
    html += "    tableBody.appendChild(row);";
    html += "  });";
    html += "}";
    
    // Update schedule field
    html += "function updateSchedule(index, field, value) {";
    html += "  if(field === 'enabled') {";
    html += "    currentSchedules[index][field] = value;";
    html += "  } else {";
    html += "    currentSchedules[index][field] = (field === 'description') ? value : parseInt(value);";
    html += "  }";
    html += "  markChanged();";
    html += "}";
    
    // Mark schedule for deletion
    html += "function markForDeletion(index) {";
    html += "  if(currentSchedules[index].isNew) {";
    html += "    currentSchedules.splice(index, 1);";
    html += "  } else {";
    html += "    currentSchedules[index].markedForDeletion = true;";
    html += "    if(!schedulesToDelete.includes(currentSchedules[index].index)) {";
    html += "      schedulesToDelete.push(currentSchedules[index].index);";
    html += "    }";
    html += "  }";
    html += "  renderScheduleTable();";
    html += "  markChanged();";
    html += "  console.log('Schedule marked for deletion (index: ' + index + ')');";
    html += "}";
    
    // Add new schedule row
    html += "function addNewScheduleRow() {";
    html += "  const newSchedule = {";
    html += "    hour: 8,";
    html += "    minute: 0,";
    html += "    second: 0,";
    html += "    portions: 2,";
    html += "    description: 'New schedule',";
    html += "    enabled: true,";
    html += "    isNew: true,";
    html += "    newId: 'new_' + (newScheduleCounter++)";
    html += "  };";
    html += "  currentSchedules.push(newSchedule);";
    html += "  renderScheduleTable();";
    html += "  markChanged();";
    html += "  console.log('New schedule row added');";
    html += "}";
    
    // Save all changes
    html += "function saveAllChanges() {";
    html += "  if(!hasUnsavedChanges) return;";
    html += "  if(!confirm('Save all changes? This will:\\n- Delete marked schedules\\n- Update modified schedules\\n- Add new schedules')) return;";
    html += "  console.log('Saving all changes...');";
    html += "  let operations = [];";
    html += "  schedulesToDelete.forEach(function(index) {";
    html += "    operations.push({type: 'delete', index: index});";
    html += "  });";
    html += "  currentSchedules.forEach(function(schedule, idx) {";
    html += "    if(schedule.markedForDeletion) return;";
    html += "    if(schedule.isNew) {";
    html += "      operations.push({type: 'add', data: schedule});";
    html += "    } else {";
    html += "      const original = originalSchedules.find(s => s.index === schedule.index);";
    html += "      if(!original || JSON.stringify(original) !== JSON.stringify(schedule)) {";
    html += "        operations.push({type: 'edit', data: schedule});";
    html += "      }";
    html += "    }";
    html += "  });";
    html += "  console.log('Operations to perform:', operations);";
    html += "  executeOperations(operations, 0);";
    html += "}";
    
    // Execute operations sequentially
    html += "function executeOperations(operations, index) {";
    html += "  if(index >= operations.length) {";
    html += "    alert('✓ All changes saved successfully!');";
    html += "    loadSchedules();";
    html += "    loadStatus();";
    html += "    return;";
    html += "  }";
    html += "  const op = operations[index];";
    html += "  let url = '';";
    html += "  if(op.type === 'delete') {";
    html += "    url = '/api/schedule/delete?index=' + op.index + '&t=' + Date.now();";
    html += "  } else if(op.type === 'add') {";
    html += "    url = '/api/schedule/add?hour=' + op.data.hour + '&minute=' + op.data.minute + '&second=' + op.data.second + '&portions=' + op.data.portions + '&description=' + encodeURIComponent(op.data.description) + '&t=' + Date.now();";
    html += "  } else if(op.type === 'edit') {";
    html += "    url = '/api/schedule/edit?index=' + op.data.index + '&hour=' + op.data.hour + '&minute=' + op.data.minute + '&second=' + op.data.second + '&portions=' + op.data.portions + '&description=' + encodeURIComponent(op.data.description) + '&t=' + Date.now();";
    html += "  }";
    html += "  console.log('Executing:', op.type, url);";
    html += "  apiGet(url, function(result) {";
    html += "    if(!result || !result.success) {";
    html += "      alert('✗ Error during ' + op.type + ': ' + (result ? result.message : 'No response'));";
    html += "      return;";
    html += "    }";
    html += "    executeOperations(operations, index + 1);";
    html += "  });";
    html += "}";
    
    // Quick feeding
    html += "function feedNowFromSelect() {";
    html += "  const portions = document.getElementById('portionSelect').value;";
    html += "  console.log('Starting feeding with', portions, 'portions');";
    html += "  apiGet('/api/feed?portions=' + portions + '&t=' + Date.now(), function(result) {";
    html += "    if(result && result.success) {";
    html += "      alert('✓ Feeding started: ' + portions + ' portions');";
    html += "      loadStatus();";
    html += "    } else {";
    html += "      alert('✗ Feeding error: ' + (result ? (result.message || result.error || 'Unknown') : 'No response'));";
    html += "    }";
    html += "  });";
    html += "}";
    
    // Toggle schedule system
    html += "function toggleScheduleSystem() {";
    html += "  if(confirm('Toggle the entire schedule system?')) {";
    html += "    console.log('Toggling schedule system - feature pending');";
    html += "    alert('Note: Use serial command SCHEDULE ENABLE/DISABLE for now');";
    html += "  }";
    html += "}";
    
    // Load status
    html += "function loadStatus() {";
    html += "  apiGet('/api/status', function(status) {";
    html += "    if(status) {";
    html += "      const lastFeedingEl = document.getElementById('lastFeedingValue');";
    html += "      if(lastFeedingEl) {";
    html += "        lastFeedingEl.textContent = status.lastFeeding;";
    html += "        lastFeedingEl.className = status.lastFeeding === 'Never' ? 'status-value status-disabled' : 'status-value';";
    html += "      }";
    html += "      const nextFeedingEl = document.getElementById('nextFeedingValue');";
    html += "      if(nextFeedingEl) {";
    html += "        nextFeedingEl.textContent = status.nextFeeding;";
    html += "        nextFeedingEl.className = status.nextFeeding.includes('No active') ? 'status-value status-disabled' : 'status-value';";
    html += "      }";
    html += "      const toleranceEl = document.getElementById('tolerance');";
    html += "      if(toleranceEl) toleranceEl.value = status.tolerance;";
    html += "      const recoveryEl = document.getElementById('recovery');";
    html += "      if(recoveryEl) recoveryEl.value = status.recovery;";
    html += "    }";
    html += "  });";
    html += "}";
    
    // Refresh all data
    html += "function refreshData() {";
    html += "  if(hasUnsavedChanges && !confirm('You have unsaved changes. Refreshing will discard them. Continue?')) return;";
    html += "  loadSchedules();";
    html += "  loadStatus();";
    html += "}";
    
    // Clock update
    html += "function updateClock() {";
    html += "  const now = new Date();";
    html += "  const day = String(now.getDate()).padStart(2, '0');";
    html += "  const month = String(now.getMonth() + 1).padStart(2, '0');";
    html += "  const year = now.getFullYear();";
    html += "  const hours = String(now.getHours()).padStart(2, '0');";
    html += "  const minutes = String(now.getMinutes()).padStart(2, '0');";
    html += "  const seconds = String(now.getSeconds()).padStart(2, '0');";
    html += "  const days = ['Domingo', 'Segunda', 'Terça', 'Quarta', 'Quinta', 'Sexta', 'Sábado'];";
    html += "  const dayName = days[now.getDay()];";
    html += "  const dateTimeStr = day + '/' + month + '/' + year + ' ' + hours + ':' + minutes + ':' + seconds + ' - ' + dayName;";
    html += "  document.getElementById('current-datetime').textContent = dateTimeStr;";
    html += "}";
    
    // Initialize on page load
    html += "window.onload = function() {";
    html += "  loadSchedules();";
    html += "  loadStatus();";
    html += "  updateClock();";
    html += "  setInterval(updateClock, 1000);";
    html += "  window.addEventListener('beforeunload', function(e) {";
    html += "    if(hasUnsavedChanges) {";
    html += "      e.preventDefault();";
    html += "      e.returnValue = 'You have unsaved changes. Are you sure you want to leave?';";
    html += "    }";
    html += "  });";
    html += "};";
    html += "console.log('Schedule management page loaded with inline editing');";
    
    html += "</script>";
    html += "</body></html>";
    
    Console::printlnR("=== SCHEDULE MANAGEMENT PAGE GENERATED (" + String(html.length()) + " bytes) ===");
    return html;
}

/**
 * Setup API endpoints for Schedule Management
 */
void WiFiController::setupScheduleAPIEndpoints() {
    Console::printlnR("=== SETTING UP SCHEDULE API ENDPOINTS ===");
    
    // CRITICAL: Check modules pointer FIRST
    if (!modules) {
        Console::printlnR(F("❌ CRITICAL ERROR: ModuleManager not set!"));
        return;
    }
    
    if (!modules->getFeedingSchedule()) {
        Console::printlnR(F("WARNING: FeedingSchedule not available for API endpoints"));
        return;
    }
    Console::printlnR("FeedingSchedule available - setting up endpoints...");
    
    // Get schedule status (last feeding, next feeding, etc.)
    wifiManager.server->on("/api/status", HTTP_GET, [this]() {
        Console::printlnR("API: Status request received");
        String json = "{";
        
        // CRITICAL: Verify modules pointer before use
        if (modules && modules->getFeedingSchedule()) {
            // Get last feeding time
            DateTime lastFeeding = modules->getFeedingSchedule()->getLastCompletedFeeding();
            json += "\"lastFeeding\":\"";
            if (lastFeeding.year() == 2000) {
                json += "Never";
            } else {
                // Format: DD/MM/YYYY HH:MM with leading zeros
                if (lastFeeding.day() < 10) json += "0";
                json += String(lastFeeding.day()) + "/";
                if (lastFeeding.month() < 10) json += "0";
                json += String(lastFeeding.month()) + "/" + String(lastFeeding.year());
                json += " ";
                if (lastFeeding.hour() < 10) json += "0";
                json += String(lastFeeding.hour()) + ":";
                if (lastFeeding.minute() < 10) json += "0";
                json += String(lastFeeding.minute());
            }
            json += "\",";
            
            // Get next feeding time
            DateTime nextFeeding = modules->getFeedingSchedule()->getNextScheduledTime();
            json += "\"nextFeeding\":\"";
            if (nextFeeding.year() == 2000 || nextFeeding.year() >= 2099) {
                json += "No active schedules";
            } else {
                // Format: DD/MM/YYYY HH:MM with leading zeros
                if (nextFeeding.day() < 10) json += "0";
                json += String(nextFeeding.day()) + "/";
                if (nextFeeding.month() < 10) json += "0";
                json += String(nextFeeding.month()) + "/" + String(nextFeeding.year());
                json += " ";
                if (nextFeeding.hour() < 10) json += "0";
                json += String(nextFeeding.hour()) + ":";
                if (nextFeeding.minute() < 10) json += "0";
                json += String(nextFeeding.minute());
            }
            json += "\",";
            
            json += "\"scheduleEnabled\":";
            json += modules->getFeedingSchedule()->isScheduleEnabled() ? "true" : "false";
            json += ",\"scheduleCount\":";
            json += String(modules->getFeedingSchedule()->getScheduleCount());
            json += ",\"tolerance\":";
            json += String(modules->getFeedingSchedule()->getTolerance());
            json += ",\"recovery\":";
            json += String(modules->getFeedingSchedule()->getMaxRecoveryHours());
        } else {
            json += "\"lastFeeding\":\"System offline\"";
            json += ",\"nextFeeding\":\"System offline\"";
            json += ",\"scheduleEnabled\":false";
            json += ",\"scheduleCount\":0";
            json += ",\"tolerance\":30";
            json += ",\"recovery\":12";
        }
        
        json += "}";
        Console::printlnR("API: Status response sent - " + json.substring(0, 100) + (json.length() > 100 ? "..." : ""));
        wifiManager.server->send(200, "application/json", json);
    });
    
    // Get all schedules
    wifiManager.server->on("/api/schedules", HTTP_GET, [this]() {
        Console::printlnR("API: Schedules request received");
        String json = "[";
        
        if (modules && modules->getFeedingSchedule() && modules->getFeedingSchedule()->getScheduleCount() > 0) {
            for (uint8_t i = 0; i < modules->getFeedingSchedule()->getScheduleCount(); i++) {
                ScheduledFeeding schedule = modules->getFeedingSchedule()->getSchedule(i);
                
                if (i > 0) json += ",";
                json += "{";
                json += "\"index\":" + String(i) + ",";
                json += "\"hour\":" + String(schedule.hour) + ",";
                json += "\"minute\":" + String(schedule.minute) + ",";
                json += "\"second\":" + String(schedule.second) + ",";
                json += "\"portions\":" + String(schedule.portions) + ",";
                json += "\"enabled\":" + String(schedule.enabled ? "true" : "false") + ",";
                json += "\"description\":\"" + String(schedule.description) + "\"";
                json += "}";
            }
        }
        
        json += "]";
        Console::printlnR("API: Schedules response sent - " + String(modules && modules->getFeedingSchedule() ? modules->getFeedingSchedule()->getScheduleCount() : 0) + " schedules");
        wifiManager.server->send(200, "application/json", json);
    });
    
    // Manual feeding endpoint - GET method (WiFiManager POST workaround)
    Console::printlnR("Registering /api/feed endpoint (GET method)...");
    wifiManager.server->on("/api/feed", HTTP_GET, [this]() {
        Console::printlnR("=== API FEED REQUEST RECEIVED (GET) ===");
        Console::printlnR("Request method: GET");
        Console::printlnR("Request URI: " + wifiManager.server->uri());
        Console::printlnR("Client IP: " + wifiManager.server->client().remoteIP().toString());
        
        // Log all received arguments
        Console::printlnR("Total arguments: " + String(wifiManager.server->args()));
        for (int i = 0; i < wifiManager.server->args(); i++) {
            Console::printlnR("  [" + String(i) + "] " + wifiManager.server->argName(i) + " = '" + wifiManager.server->arg(i) + "'");
        }
        
        // Get portions parameter from URL query string
        String portionsStr = "";
        int portions = 0;
        
        if (wifiManager.server->hasArg("portions")) {
            portionsStr = wifiManager.server->arg("portions");
            Console::printlnR("Found portions parameter: '" + portionsStr + "'");
        } else {
            Console::printlnR("ERROR: Missing 'portions' parameter in URL");
            wifiManager.server->send(400, "application/json", "{\"success\":false,\"message\":\"Missing 'portions' parameter. Use: /api/feed?portions=X\"}");
            return;
        }
        
        portions = portionsStr.toInt();
        Console::printlnR("Final parsed portions: " + String(portions));
        
        if (portions < 1 || portions > 20) {
            Console::printlnR("ERROR: Invalid portions count - " + String(portions));
            wifiManager.server->send(400, "text/plain", "Invalid portions count (1-20)");
            return;
        }
        
        // Check components availability
        Console::printlnR("Checking system components:");
        Console::printlnR("  modules->getFeedingController() available: " + String(modules->getFeedingController() ? "YES" : "NO"));
        
        if (modules && modules->getFeedingController()) {
            Console::printlnR("  modules->getFeedingController() ready: " + String(modules->getFeedingController()->isReady() ? "YES" : "NO"));
        }
        
        Console::printlnR("  modules->getFeedingSchedule() available: " + String(modules->getFeedingSchedule() ? "YES" : "NO"));
        
        // Execute feeding via modules->getFeedingController()
        if (modules && modules->getFeedingController() && modules->getFeedingController()->isReady()) {
            Console::printlnR("Attempting to start feeding with " + String(portions) + " portions...");
            bool success = modules->getFeedingController()->dispenseFoodAsync(portions);
            Console::printlnR("modules->getFeedingController()->dispenseFoodAsync() result: " + String(success ? "SUCCESS" : "FAILED"));
            
            if (success) {
                // Update last feeding time in schedule with current time
                if (modules && modules->getFeedingSchedule()) {
                    Console::printlnR("Recording manual feeding in schedule...");
                    // Use current timestamp - modules->getFeedingSchedule() will handle time conversion
                    DateTime now = DateTime(millis() / 1000 + 946684800); // Convert millis to Unix timestamp
                    modules->getFeedingSchedule()->recordManualFeeding(now);
                    Console::printlnR("Manual feeding recorded successfully");
                }
                
                String response = "{\"success\":true,\"message\":\"Started feeding " + String(portions) + " portions\"}";
                wifiManager.server->send(200, "application/json", response);
                
                Console::printlnR("API: Manual feeding started successfully - " + String(portions) + " portions");
                Console::printlnR("=== API FEED REQUEST COMPLETED ===");
            } else {
                String response = "{\"success\":false,\"message\":\"Failed to start feeding - controller not ready\"}";
                wifiManager.server->send(500, "application/json", response);
                
                Console::printlnR("API: Manual feeding failed - dispenseFoodAsync returned false");
                Console::printlnR("=== API FEED REQUEST FAILED ===");
            }
        } else {
            if (!modules->getFeedingController()) {
                Console::printlnR("ERROR: modules->getFeedingController() pointer is NULL");
            } else if (!modules->getFeedingController()->isReady()) {
                Console::printlnR("ERROR: modules->getFeedingController() is not ready");
            }
            
            String response = "{\"success\":false,\"message\":\"Feeding controller not available\"}";
            wifiManager.server->send(503, "application/json", response);
            
            Console::printlnR("API: Manual feeding rejected - controller unavailable");
            Console::printlnR("=== API FEED REQUEST REJECTED ===");
        }
    });
    
    // Toggle schedule system
    wifiManager.server->on("/api/schedule/toggle", HTTP_POST, [this]() {
        if (!modules || !modules->getFeedingSchedule()) {
            wifiManager.server->send(500, "application/json", "{\"success\":false,\"message\":\"Schedule system not available\"}");
            return;
        }
        
        bool currentState = modules->getFeedingSchedule()->isScheduleEnabled();
        modules->getFeedingSchedule()->enableSchedule(!currentState);
        
        String json = "{\"success\":true,\"enabled\":" + String(!currentState ? "true" : "false") + "}";
        wifiManager.server->send(200, "application/json", json);
        
        Console::printlnR("API: Schedule system " + String(!currentState ? "enabled" : "disabled"));
    });
    
    // Toggle individual schedule
    wifiManager.server->on("/api/schedule/toggle-item", HTTP_POST, [this]() {
        if (!modules || !modules->getFeedingSchedule()) {
            wifiManager.server->send(500, "application/json", "{\"success\":false,\"message\":\"Schedule system not available\"}");
            return;
        }
        
        if (!wifiManager.server->hasArg("index")) {
            wifiManager.server->send(400, "text/plain", "Missing index parameter");
            return;
        }
        
        int index = wifiManager.server->arg("index").toInt();
        if (index < 0 || index >= modules->getFeedingSchedule()->getScheduleCount()) {
            wifiManager.server->send(400, "text/plain", "Invalid schedule index");
            return;
        }
        
        bool currentState = modules->getFeedingSchedule()->isScheduleEnabled(index);
        modules->getFeedingSchedule()->enableScheduleAtIndex(index, !currentState);
        
        String json = "{\"success\":true,\"enabled\":" + String(!currentState ? "true" : "false") + "}";
        wifiManager.server->send(200, "application/json", json);
        
        Console::printlnR("API: Schedule " + String(index) + " " + String(!currentState ? "enabled" : "disabled"));
    });
    
    // Set tolerance
    wifiManager.server->on("/api/schedule/tolerance", HTTP_POST, [this]() {
        if (!modules || !modules->getFeedingSchedule()) {
            wifiManager.server->send(500, "application/json", "{\"success\":false,\"message\":\"Schedule system not available\"}");
            return;
        }
        
        if (!wifiManager.server->hasArg("minutes")) {
            wifiManager.server->send(400, "text/plain", "Missing minutes parameter");
            return;
        }
        
        int minutes = wifiManager.server->arg("minutes").toInt();
        if (minutes < 1 || minutes > 120) {
            wifiManager.server->send(400, "text/plain", "Invalid tolerance (1-120 minutes)");
            return;
        }
        
        modules->getFeedingSchedule()->setTolerance(minutes);
        
        String json = "{\"success\":true,\"tolerance\":" + String(minutes) + "}";
        wifiManager.server->send(200, "application/json", json);
        
        Console::printlnR("API: Tolerance set to " + String(minutes) + " minutes");
    });
    
    // Set recovery period
    wifiManager.server->on("/api/schedule/recovery", HTTP_POST, [this]() {
        if (!modules || !modules->getFeedingSchedule()) {
            wifiManager.server->send(500, "application/json", "{\"success\":false,\"message\":\"Schedule system not available\"}");
            return;
        }
        
        if (!wifiManager.server->hasArg("hours")) {
            wifiManager.server->send(400, "text/plain", "Missing hours parameter");
            return;
        }
        
        int hours = wifiManager.server->arg("hours").toInt();
        if (hours < 1 || hours > 72) {
            wifiManager.server->send(400, "text/plain", "Invalid recovery period (1-72 hours)");
            return;
        }
        
        modules->getFeedingSchedule()->setMaxRecoveryHours(hours);
        
        String json = "{\"success\":true,\"recovery\":" + String(hours) + "}";
        wifiManager.server->send(200, "application/json", json);
        
        Console::printlnR("API: Recovery period set to " + String(hours) + " hours");
    });
    
    // Add new schedule - GET method for WiFiManager compatibility
    wifiManager.server->on("/api/schedule/add", HTTP_GET, [this]() {
        Console::printlnR("=== API ADD SCHEDULE REQUEST ===");
        
        if (!wifiManager.server->hasArg("hour") || !wifiManager.server->hasArg("minute") || 
            !wifiManager.server->hasArg("second") || !wifiManager.server->hasArg("portions")) {
            wifiManager.server->send(400, "application/json", 
                "{\"success\":false,\"message\":\"Missing required parameters\"}");
            return;
        }
        
        int hour = wifiManager.server->arg("hour").toInt();
        int minute = wifiManager.server->arg("minute").toInt();
        int second = wifiManager.server->arg("second").toInt();
        int portions = wifiManager.server->arg("portions").toInt();
        String description = wifiManager.server->hasArg("description") ? 
                           wifiManager.server->arg("description") : "";
        
        Console::printlnR("Adding schedule: " + String(hour) + ":" + String(minute) + ":" + 
                         String(second) + " - " + String(portions) + " portions");
        
        if (modules && modules->getFeedingSchedule() && modules->getFeedingSchedule()->addSchedule(hour, minute, second, portions, description.c_str())) {
            String json = "{\"success\":true,\"message\":\"Schedule added successfully\"}";
            wifiManager.server->send(200, "application/json", json);
            Console::printlnR("API: Schedule added successfully");
        } else {
            String json = "{\"success\":false,\"message\":\"Failed to add schedule\"}";
            wifiManager.server->send(500, "application/json", json);
            Console::printlnR("API: Failed to add schedule");
        }
    });
    
    // Edit existing schedule - GET method for WiFiManager compatibility
    wifiManager.server->on("/api/schedule/edit", HTTP_GET, [this]() {
        Console::printlnR("=== API EDIT SCHEDULE REQUEST ===");
        
        if (!wifiManager.server->hasArg("index") || !wifiManager.server->hasArg("hour") || 
            !wifiManager.server->hasArg("minute") || !wifiManager.server->hasArg("second") || 
            !wifiManager.server->hasArg("portions")) {
            wifiManager.server->send(400, "application/json", 
                "{\"success\":false,\"message\":\"Missing required parameters\"}");
            return;
        }
        
        int index = wifiManager.server->arg("index").toInt();
        int hour = wifiManager.server->arg("hour").toInt();
        int minute = wifiManager.server->arg("minute").toInt();
        int second = wifiManager.server->arg("second").toInt();
        int portions = wifiManager.server->arg("portions").toInt();
        String description = wifiManager.server->hasArg("description") ? 
                           wifiManager.server->arg("description") : "";
        
        Console::printlnR("Editing schedule " + String(index) + ": " + String(hour) + ":" + 
                         String(minute) + ":" + String(second) + " - " + String(portions) + " portions");
        
        if (modules && modules->getFeedingSchedule() && modules->getFeedingSchedule()->editSchedule(index, hour, minute, second, portions, description.c_str())) {
            String json = "{\"success\":true,\"message\":\"Schedule updated successfully\"}";
            wifiManager.server->send(200, "application/json", json);
            Console::printlnR("API: Schedule edited successfully");
        } else {
            String json = "{\"success\":false,\"message\":\"Failed to edit schedule\"}";
            wifiManager.server->send(500, "application/json", json);
            Console::printlnR("API: Failed to edit schedule");
        }
    });
    
    // Delete schedule - GET method for WiFiManager compatibility
    wifiManager.server->on("/api/schedule/delete", HTTP_GET, [this]() {
        Console::printlnR("=== API DELETE SCHEDULE REQUEST ===");
        
        if (!wifiManager.server->hasArg("index")) {
            wifiManager.server->send(400, "application/json", 
                "{\"success\":false,\"message\":\"Missing index parameter\"}");
            return;
        }
        
        int index = wifiManager.server->arg("index").toInt();
        
        Console::printlnR("Deleting schedule " + String(index));
        
        if (modules && modules->getFeedingSchedule() && modules->getFeedingSchedule()->removeSchedule(index)) {
            String json = "{\"success\":true,\"message\":\"Schedule deleted successfully\"}";
            wifiManager.server->send(200, "application/json", json);
            Console::printlnR("API: Schedule deleted successfully");
        } else {
            String json = "{\"success\":false,\"message\":\"Failed to delete schedule\"}";
            wifiManager.server->send(500, "application/json", json);
            Console::printlnR("API: Failed to delete schedule");
        }
    });
    
    Console::printlnR("=== SCHEDULE API ENDPOINTS SETUP COMPLETE ===");
    Console::printlnR("Endpoints registered: /api/status, /api/schedules, /api/feed, /api/schedule/*, etc.");
}

