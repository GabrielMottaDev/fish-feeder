/**
 * ModuleManager - Centralized Module Reference Management
 * 
 * This class implements a Service Locator pattern to manage all system module references.
 * Instead of each module receiving multiple individual module pointers, modules receive
 * a single ModuleManager reference containing all module references.
 * 
 * Benefits:
 * - Reduces coupling between modules
 * - Simplifies module initialization
 * - Provides centralized module access
 * - Easier to add new modules without changing existing code
 * - Type-safe module access with compile-time checking
 * 
 * Architecture Pattern: Service Locator with Singleton access
 */

#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <Arduino.h>

// Forward declarations to avoid circular dependencies
class RTCModule;
class StepperMotor;
class FeedingController;
class FeedingSchedule;
class WiFiController;
class NTPSync;
class VibrationMotor;
class RGBLed;
class TouchSensor;

/**
 * ModuleManager - Central registry for all system modules
 * 
 * This class maintains references to all system modules and provides
 * type-safe access methods for each module.
 */
class ModuleManager {
public:
    /**
     * Constructor - Initializes all module pointers to nullptr
     */
    ModuleManager();
    
    // ========================================================================
    // MODULE REGISTRATION METHODS
    // ========================================================================
    
    /**
     * Register RTC module
     * @param module Pointer to RTCModule instance
     */
    void registerRTCModule(RTCModule* module);
    
    /**
     * Register stepper motor
     * @param motor Pointer to StepperMotor instance
     */
    void registerStepperMotor(StepperMotor* motor);
    
    /**
     * Register feeding controller
     * @param controller Pointer to FeedingController instance
     */
    void registerFeedingController(FeedingController* controller);
    
    /**
     * Register feeding schedule system
     * @param schedule Pointer to FeedingSchedule instance
     */
    void registerFeedingSchedule(FeedingSchedule* schedule);
    
    /**
     * Register WiFi controller
     * @param controller Pointer to WiFiController instance
     */
    void registerWiFiController(WiFiController* controller);
    
    /**
     * Register NTP sync module
     * @param sync Pointer to NTPSync instance
     */
    void registerNTPSync(NTPSync* sync);
    
    /**
     * Register vibration motor
     * @param motor Pointer to VibrationMotor instance
     */
    void registerVibrationMotor(VibrationMotor* motor);
    
    /**
     * Register RGB LED module
     * @param led Pointer to RGBLed instance
     */
    void registerRGBLed(RGBLed* led);
    
    /**
     * Register touch sensor
     * @param sensor Pointer to TouchSensor instance
     */
    void registerTouchSensor(TouchSensor* sensor);
    
    // ========================================================================
    // MODULE ACCESSOR METHODS
    // ========================================================================
    
    /**
     * Get RTC module reference
     * @return Pointer to RTCModule instance (may be nullptr if not registered)
     */
    RTCModule* getRTCModule() const { return rtcModule; }
    
    /**
     * Get stepper motor reference
     * @return Pointer to StepperMotor instance (may be nullptr if not registered)
     */
    StepperMotor* getStepperMotor() const { return stepperMotor; }
    
    /**
     * Get feeding controller reference
     * @return Pointer to FeedingController instance (may be nullptr if not registered)
     */
    FeedingController* getFeedingController() const { return feedingController; }
    
    /**
     * Get feeding schedule reference
     * @return Pointer to FeedingSchedule instance (may be nullptr if not registered)
     */
    FeedingSchedule* getFeedingSchedule() const { return feedingSchedule; }
    
    /**
     * Get WiFi controller reference
     * @return Pointer to WiFiController instance (may be nullptr if not registered)
     */
    WiFiController* getWiFiController() const { return wifiController; }
    
    /**
     * Get NTP sync reference
     * @return Pointer to NTPSync instance (may be nullptr if not registered)
     */
    NTPSync* getNTPSync() const { return ntpSync; }
    
    /**
     * Get vibration motor reference
     * @return Pointer to VibrationMotor instance (may be nullptr if not registered)
     */
    VibrationMotor* getVibrationMotor() const { return vibrationMotor; }
    
    /**
     * Get RGB LED reference
     * @return Pointer to RGBLed instance (may be nullptr if not registered)
     */
    RGBLed* getRGBLed() const { return rgbLed; }
    
    /**
     * Get touch sensor reference
     * @return Pointer to TouchSensor instance (may be nullptr if not registered)
     */
    TouchSensor* getTouchSensor() const { return touchSensor; }
    
    // ========================================================================
    // MODULE AVAILABILITY CHECK
    // ========================================================================
    
    /**
     * Check if RTC module is registered
     * @return true if module is available, false otherwise
     */
    bool hasRTCModule() const { return rtcModule != nullptr; }
    
    /**
     * Check if stepper motor is registered
     * @return true if module is available, false otherwise
     */
    bool hasStepperMotor() const { return stepperMotor != nullptr; }
    
    /**
     * Check if feeding controller is registered
     * @return true if module is available, false otherwise
     */
    bool hasFeedingController() const { return feedingController != nullptr; }
    
    /**
     * Check if feeding schedule is registered
     * @return true if module is available, false otherwise
     */
    bool hasFeedingSchedule() const { return feedingSchedule != nullptr; }
    
    /**
     * Check if WiFi controller is registered
     * @return true if module is available, false otherwise
     */
    bool hasWiFiController() const { return wifiController != nullptr; }
    
    /**
     * Check if NTP sync is registered
     * @return true if module is available, false otherwise
     */
    bool hasNTPSync() const { return ntpSync != nullptr; }
    
    /**
     * Check if vibration motor is registered
     * @return true if module is available, false otherwise
     */
    bool hasVibrationMotor() const { return vibrationMotor != nullptr; }
    
    /**
     * Check if RGB LED is registered
     * @return true if module is available, false otherwise
     */
    bool hasRGBLed() const { return rgbLed != nullptr; }
    
    /**
     * Check if touch sensor is registered
     * @return true if module is available, false otherwise
     */
    bool hasTouchSensor() const { return touchSensor != nullptr; }
    
    // ========================================================================
    // GLOBAL FEEDING STATE (moved from main.cpp)
    // ========================================================================
    
    /**
     * Set global feeding state
     * @param feeding true if feeding is in progress, false otherwise
     */
    void setFeedingInProgress(bool feeding) { feedingInProgress = feeding; }
    
    /**
     * Get global feeding state
     * @return true if feeding is in progress, false otherwise
     */
    bool getFeedingInProgress() const { return feedingInProgress; }
    
private:
    // Module references
    RTCModule* rtcModule;
    StepperMotor* stepperMotor;
    FeedingController* feedingController;
    FeedingSchedule* feedingSchedule;
    WiFiController* wifiController;
    NTPSync* ntpSync;
    VibrationMotor* vibrationMotor;
    RGBLed* rgbLed;
    TouchSensor* touchSensor;
    
    // Global feeding state
    bool feedingInProgress;
};

#endif // MODULE_MANAGER_H
