/**
 * ModuleManager - Centralized Module Reference Management
 * 
 * Implementation file for the ModuleManager class.
 * Provides centralized registration and access to all system modules.
 */

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

/**
 * Constructor - Initialize all module pointers to nullptr
 */
ModuleManager::ModuleManager() 
    : rtcModule(nullptr),
      stepperMotor(nullptr),
      feedingController(nullptr),
      feedingSchedule(nullptr),
      wifiController(nullptr),
      ntpSync(nullptr),
      vibrationMotor(nullptr),
      rgbLed(nullptr),
      touchSensor(nullptr),
      feedingInProgress(false) {
}

// ============================================================================
// MODULE REGISTRATION IMPLEMENTATIONS
// ============================================================================

void ModuleManager::registerRTCModule(RTCModule* module) {
    rtcModule = module;
}

void ModuleManager::registerStepperMotor(StepperMotor* motor) {
    stepperMotor = motor;
}

void ModuleManager::registerFeedingController(FeedingController* controller) {
    feedingController = controller;
}

void ModuleManager::registerFeedingSchedule(FeedingSchedule* schedule) {
    feedingSchedule = schedule;
}

void ModuleManager::registerWiFiController(WiFiController* controller) {
    wifiController = controller;
}

void ModuleManager::registerNTPSync(NTPSync* sync) {
    ntpSync = sync;
}

void ModuleManager::registerVibrationMotor(VibrationMotor* motor) {
    vibrationMotor = motor;
}

void ModuleManager::registerRGBLed(RGBLed* led) {
    rgbLed = led;
}

void ModuleManager::registerTouchSensor(TouchSensor* sensor) {
    touchSensor = sensor;
}
