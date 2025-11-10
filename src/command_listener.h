#ifndef COMMAND_LISTENER_H
#define COMMAND_LISTENER_H

#include <Arduino.h>
#include "config.h"

// Forward declarations
class ModuleManager;

/**
 * CommandListener Class
 * 
 * Manages all command processing and serial command interpretation.
 * This class centralizes all command logic, help display, and
 * command execution for the Fish Feeder system.
 * 
 * Architecture:
 * - Uses ModuleManager for accessing all system modules
 * - Simplifies constructor and reduces coupling
 */
class CommandListener {
private:
    ModuleManager* modules;
    
    // Command processing methods
    bool processSystemCommands(const String& command);
    bool processMotorCommands(const String& command);
    bool processTaskCommands(const String& command);
    bool processRTCCommands(const String& command);
    bool processWiFiCommands(const String& command);
    bool processNTPCommands(const String& command);
    bool processScheduleCommands(const String& command);
    bool processVibrationCommands(const String& command);
    bool processRGBCommands(const String& command);
    bool processTouchCommands(const String& command);
    
public:
    // Constructor
    CommandListener(ModuleManager* moduleManager);
    
    // Main command processing
    bool processCommand(const String& command);
    
    // Help and status display
    void showHelp();
    void showSystemInfo();
    void showWiFiPortalConfig();
};

#endif // COMMAND_LISTENER_H