#ifndef COMMAND_LISTENER_H
#define COMMAND_LISTENER_H

#include <Arduino.h>
#include "rtc_module.h"
#include "stepper_motor.h"
#include "feeding_controller.h"
#include "feeding_schedule.h"
#include "wifi_controller.h"
#include "ntp_sync.h"
#include "vibration_motor.h"
#include "rgb_led.h"
#include "config.h"

/**
 * CommandListener Class
 * 
 * Manages all command processing and serial command interpretation.
 * This class centralizes all command logic, help display, and
 * command execution for the Fish Feeder system.
 */
class CommandListener {
private:
    RTCModule* rtcModule;
    StepperMotor* stepperMotor;
    FeedingController* feedingController;
    FeedingSchedule* feedingSchedule;
    WiFiController* wifiController;
    NTPSync* ntpSync;
    VibrationMotor* vibrationMotor;
    RGBLed* rgbLed;
    bool* feedingInProgress;
    
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
    
public:
    // Constructor
    CommandListener(RTCModule* rtc, StepperMotor* motor, 
                   FeedingController* feeder, FeedingSchedule* schedule,
                   WiFiController* wifi, NTPSync* ntp, 
                   VibrationMotor* vibration, RGBLed* rgb, bool* feedingState);
    
    // Main command processing
    bool processCommand(const String& command);
    
    // Help and status display
    void showHelp();
    void showSystemInfo();
    void showWiFiPortalConfig();
};

#endif // COMMAND_LISTENER_H