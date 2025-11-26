#ifndef WIFI_CONTROLLER_H
#define WIFI_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h> // tzapu WiFiManager library
#include "config.h"

// Forward declarations
class ModuleManager;
class FeedingSchedule;
class FeedingController;
class RGBLed;

/**
 * WiFiController Class
 * 
 * Manages WiFi functionality for ESP32 using tzapu's WiFiManager
 * Combines custom WiFi controls with tzapu's powerful captive portal
 * Provides complete control over wireless connections including:
 * - WiFi network scanning and connection management
 * - Captive portal for easy configuration via web interface
 * - Saved network credentials storage using Preferences
 * - Custom parameters integration
 * - Network status monitoring and diagnostics
 * 
 * Architecture:
 * - Uses ModuleManager for accessing other system modules
 * - Reduces coupling between modules
 */
class WiFiController {
private:
    Preferences preferences;
    WiFiManager wifiManager; // tzapu WiFiManager instance
    bool wifiEnabled;
    bool configPortalActive;
    
    // Reference to ModuleManager
    ModuleManager* modules;
    
    // Reference to RGB LED for status indication
    RGBLed* rgbLed;
    
    // WiFi connection state
    String currentSSID;
    bool isConnected;
    unsigned long lastConnectionAttempt;
    unsigned long lastConnectionCheck;
    bool wasConnectedBefore;
    
    // Non-blocking connection state machine
    enum WiFiConnectionState {
        WIFI_IDLE,
        WIFI_DISCONNECTING,
        WIFI_CONNECTING,
        WIFI_CONNECTED,
        WIFI_FAILED
    };
    WiFiConnectionState connectionState;
    unsigned long connectionStateTime;
    int connectionAttempts;
    static const int MAX_CONNECTION_ATTEMPTS = 20; // 10 seconds total
    static const unsigned long CONNECTION_CHECK_INTERVAL = 500; // Check every 500ms
    
    // Pending connection parameters for state machine
    String pendingSSID;
    String pendingPassword;
    bool pendingSaveCredentials;
    
    // Non-blocking portal state
    bool portalStartRequested;
    String portalAPName;
    unsigned long portalStartTime;
    bool shutdownRequested;
    
    // Internal helper methods
    void printWiFiStatus();
    void printNetworkDetails();
    String encryptionTypeStr(wifi_auth_mode_t encryptionType);
    void saveNetworkCredentials(const String& ssid, const String& password);
    bool loadNetworkCredentials(const String& ssid, String& password);
    void removeNetworkCredentials(const String& ssid);
    String getStoredNetworksKey(int index);
    
    // Non-blocking connection state machine
    void processConnectionState();
    
public:
    // Constructor
    WiFiController();
    
    // Initialization
    bool begin();
    void setModuleManager(ModuleManager* moduleManager);
    void setRGBLed(RGBLed* led);  // Set RGB LED reference for status indication
    void registerAllEndpoints(); // Register ALL endpoints after components are ready
    
    // WiFi Management
    void scanNetworks();
    bool connectToNetwork(const String& ssid, const String& password, bool saveCredentials = true);
    bool connectToSavedNetwork(const String& ssid);
    void disconnectWiFi();
    void listSavedNetworks();
    void removeSavedNetwork(const String& ssid);
    void clearAllSavedNetworks();
    
    // WiFi Status and Information
    bool isWiFiConnected();
    String getCurrentSSID();
    int getSignalStrength();
    String getLocalIP();
    String getMACAddress();
    void showWiFiStatus();
    void showNetworkInfo();
    
    // Internet connectivity test
    bool testInternetConnection();
    
    // DNS management
    void configureDNSServers();
    void testDNSServers();
    
    // Command Processing
    bool processWiFiCommand(const String& command);
    
    // Feeding Schedule Web Interface
    String generateScheduleManagementPage();
    void setupScheduleAPIEndpoints();
    
    // tzapu WiFiManager integration
    void startConfigPortal(const String& apName = "FishFeeder-Setup");
    void stopConfigPortal();
    bool isConfigPortalActive();
    void processConfigPortal(); // Non-blocking portal processing
    void startAlwaysOnPortal(); // Always-on portal using tzapu features
    
    // Auto-reconnection and portal management
    void handleAutoReconnect();
    void checkConnectionStatus();
    void startPortalOnBoot();
    void startPortalOnDisconnect();
    bool tryAutoConnect();
};

#endif // WIFI_CONTROLLER_H