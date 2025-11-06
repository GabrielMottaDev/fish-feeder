#ifndef FEEDING_SCHEDULE_H
#define FEEDING_SCHEDULE_H

#include <RTClib.h>
#include <Preferences.h>
#include "console_manager.h"
#include "feeding_controller.h"
#include "rtc_module.h"
#include "config.h"

// Forward declaration
class FeedingController;

/**
 * FeedingSchedule Class
 * 
 * Manages scheduled feeding times with the following features:
 * - Non-blocking schedule monitoring
 * - NVRAM persistence for power-loss recovery
 * - Missed feeding recovery with tolerance
 * - Integration with FeedingController for actual feeding
 * - Real-time schedule management
 */
class FeedingSchedule {
private:
    // Schedule management
    ScheduledFeeding scheduleStorage[10]; // Fixed array storage (MAX_SCHEDULED_FEEDINGS = 10)
    ScheduledFeeding* schedules;    // Pointer to current schedules (will point to scheduleStorage)
    uint8_t scheduleCount;          // Number of active schedules
    bool scheduleEnabled;           // Global enable/disable flag
    
    // Persistence and recovery
    Preferences preferences;        // NVRAM storage
    DateTime lastCompletedFeeding; // Last successful feeding time
    bool persistenceInitialized;   // NVRAM initialization status
    
    // Controller and module references
    FeedingController* feedingController;
    RTCModule* rtcModule;
    
    // State management
    bool feedingInProgress;         // Current feeding status
    DateTime nextScheduledTime;     // Next calculated feeding time
    uint8_t nextScheduleIndex;      // Index of next schedule to execute
    
    // Tolerance and recovery
    uint16_t toleranceMinutes;      // Minutes tolerance for missed feedings
    uint16_t maxRecoveryHours;      // Max hours to look back for missed feedings
    
    // Private methods
    void initializePersistence();
    void loadLastFeedingFromNVRAM();
    void saveLastFeedingToNVRAM(const DateTime& feedingTime);
    void calculateNextFeeding();
    bool isTimeForFeeding(const DateTime& currentTime, const ScheduledFeeding& schedule);
    bool isFeedingMissed(const DateTime& currentTime, const ScheduledFeeding& schedule);
    void executeFeeding(const ScheduledFeeding& schedule);
    void recoverMissedFeedings(const DateTime& currentTime);
    DateTime getScheduleDateTime(const ScheduledFeeding& schedule, const DateTime& referenceDate);
    String formatTime(const DateTime& dt);
    String formatSchedule(const ScheduledFeeding& schedule);

public:
    // Constructor and initialization
    FeedingSchedule();
    void begin(FeedingController* controller, RTCModule* rtc);
    
    // Schedule management
    void setSchedules(ScheduledFeeding* scheduleArray, uint8_t count);
    bool addSchedule(uint8_t hour, uint8_t minute, uint8_t second, uint8_t portions, const char* description = "");
    bool editSchedule(uint8_t index, uint8_t hour, uint8_t minute, uint8_t second, uint8_t portions, const char* description = "");
    bool removeSchedule(uint8_t index);
    void clearAllSchedules();
    
    // NVRAM persistence for schedules
    void loadSchedulesFromNVRAM();
    void saveSchedulesToNVRAM();
    void initializeDefaultSchedules(); // Copy DEFAULT_FEEDING_SCHEDULE to NVRAM
    
    // Schedule control
    void enableSchedule(bool enabled);
    void enableScheduleAtIndex(uint8_t index, bool enabled);
    bool isScheduleEnabled();
    bool isScheduleEnabled(uint8_t index);
    
    // Main processing (non-blocking)
    void processSchedules(const DateTime& currentTime);
    
    // Status and information
    void printScheduleStatus();
    void printScheduleList();
    void printNextFeeding();
    void printLastFeeding();
    DateTime getNextScheduledTime();
    void updateNextScheduledTime(const DateTime& currentTime);
    DateTime getLastCompletedFeeding();
    uint8_t getScheduleCount();
    ScheduledFeeding getSchedule(uint8_t index);
    
    // Configuration
    void setTolerance(uint16_t toleranceMinutes);
    void setMaxRecoveryHours(uint16_t maxHours);
    uint16_t getTolerance();
    uint16_t getMaxRecoveryHours();
    
    // Manual feeding tracking
    void recordManualFeeding(const DateTime& feedingTime);
    
    // Debug and diagnostics
    void printDiagnostics();
    void testScheduleCalculation();
};

#endif // FEEDING_SCHEDULE_H