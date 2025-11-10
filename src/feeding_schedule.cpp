#include "feeding_schedule.h"
#include "module_manager.h"
#include "feeding_controller.h"
#include "rtc_module.h"
#include "config.h"

/**
 * Constructor
 */
FeedingSchedule::FeedingSchedule() :
    schedules(scheduleStorage), // Point to internal storage
    scheduleCount(0),
    scheduleEnabled(true),
    lastCompletedFeeding(DateTime(2000, 1, 1, 0, 0, 0)), // Default old date
    persistenceInitialized(false),
    modules(nullptr),
    feedingInProgress(false),
    nextScheduledTime(DateTime(2000, 1, 1, 0, 0, 0)),
    nextScheduleIndex(0),
    toleranceMinutes(FEEDING_SCHEDULE_TOLERANCE_MINUTES),
    maxRecoveryHours(FEEDING_SCHEDULE_MAX_RECOVERY_HOURS)
{
    // Constructor - initialization done in begin()
    // Clear schedule storage
    memset(scheduleStorage, 0, sizeof(scheduleStorage));
}

/**
 * Initialize the feeding schedule system
 */
void FeedingSchedule::begin(ModuleManager* moduleManager) {
    modules = moduleManager;
    initializePersistence();
    loadLastFeedingFromNVRAM();
    
    // Load schedules from NVRAM or initialize with defaults
    loadSchedulesFromNVRAM();
    
    Console::printlnR(F("FeedingSchedule: System initialized"));
    Console::printlnR("Last feeding: " + formatTime(lastCompletedFeeding));
    Console::printlnR("Active schedules: " + String(scheduleCount));
}

/**
 * Initialize NVRAM persistence
 */
void FeedingSchedule::initializePersistence() {
    if (!preferences.begin("feeding_sched", false)) {
        Console::printlnR(F("FeedingSchedule: ERROR - Failed to initialize NVRAM"));
        persistenceInitialized = false;
        return;
    }
    
    persistenceInitialized = true;
    Console::printlnR(F("FeedingSchedule: NVRAM persistence initialized"));
}

/**
 * Load last feeding time from NVRAM
 */
void FeedingSchedule::loadLastFeedingFromNVRAM() {
    if (!persistenceInitialized) {
        Console::printlnR(F("FeedingSchedule: WARNING - NVRAM not initialized, using default date"));
        return;
    }
    
    // Load Unix timestamp from NVRAM
    uint32_t lastFeedingUnix = preferences.getUInt("last_feeding", 0);
    
    if (lastFeedingUnix > 0) {
        lastCompletedFeeding = DateTime(lastFeedingUnix);
        Console::printlnR("FeedingSchedule: Loaded last feeding from NVRAM: " + formatTime(lastCompletedFeeding));
    } else {
        Console::printlnR(F("FeedingSchedule: No previous feeding record found in NVRAM"));
    }
}

/**
 * Save last feeding time to NVRAM
 */
void FeedingSchedule::saveLastFeedingToNVRAM(const DateTime& feedingTime) {
    if (!persistenceInitialized) {
        Console::printlnR(F("FeedingSchedule: WARNING - Cannot save to NVRAM, not initialized"));
        return;
    }
    
    uint32_t feedingUnix = feedingTime.unixtime();
    if (preferences.putUInt("last_feeding", feedingUnix)) {
        Console::printlnR("FeedingSchedule: Saved feeding time to NVRAM: " + formatTime(feedingTime));
    } else {
        Console::printlnR(F("FeedingSchedule: ERROR - Failed to save feeding time to NVRAM"));
    }
}

/**
 * Set schedule array (main configuration method)
 * This method is now primarily for programmatic initialization
 * Normal operation should use loadSchedulesFromNVRAM()
 */
void FeedingSchedule::setSchedules(ScheduledFeeding* scheduleArray, uint8_t count) {
    if (count > 10) { // MAX_SCHEDULED_FEEDINGS
        Console::printlnR(F("FeedingSchedule: ERROR - Too many schedules (max 10)"));
        count = 10;
    }
    
    // Copy schedules to internal storage
    scheduleCount = 0;
    for (uint8_t i = 0; i < count; i++) {
        scheduleStorage[i] = scheduleArray[i];
        scheduleCount++;
    }
    
    // Save to NVRAM
    saveSchedulesToNVRAM();
    calculateNextFeeding();
    
    Console::printlnR("FeedingSchedule: Loaded " + String(count) + " scheduled feedings");
    printScheduleList();
}

/**
 * Add a single schedule
 */
bool FeedingSchedule::addSchedule(uint8_t hour, uint8_t minute, uint8_t second, uint8_t portions, const char* description) {
    if (scheduleCount >= 10) { // MAX_SCHEDULED_FEEDINGS
        Console::printlnR(F("FeedingSchedule: ERROR - Maximum schedules reached (10)"));
        return false;
    }
    
    if (hour > 23 || minute > 59 || second > 59 || portions < 1 || portions > 10) {
        Console::printlnR(F("FeedingSchedule: ERROR - Invalid schedule parameters"));
        return false;
    }
    
    // Add new schedule to internal storage
    scheduleStorage[scheduleCount].hour = hour;
    scheduleStorage[scheduleCount].minute = minute;
    scheduleStorage[scheduleCount].second = second;
    scheduleStorage[scheduleCount].portions = portions;
    scheduleStorage[scheduleCount].enabled = true;
    strncpy(scheduleStorage[scheduleCount].description, description, 49);
    scheduleStorage[scheduleCount].description[49] = '\0'; // Ensure null termination
    
    scheduleCount++;
    
    // Save to NVRAM
    saveSchedulesToNVRAM();
    calculateNextFeeding();
    
    Console::printlnR("FeedingSchedule: Schedule added - " + String(hour) + ":" + String(minute) + ":" + String(second));
    return true;
}

/**
 * Calculate next feeding time based on current schedules
 */
void FeedingSchedule::calculateNextFeeding() {
    if (scheduleCount == 0 || !schedules) {
        nextScheduledTime = DateTime(2099, 12, 31, 23, 59, 59); // Far future
        nextScheduleIndex = 0;
        return;
    }
    
    // Get current time from RTC
    DateTime now;
    if (modules && modules->hasRTCModule()) {
        now = modules->getRTCModule()->now();
    } else {
        now = DateTime(); // Fallback to default if RTC not available
    }
    
    DateTime todayBase = DateTime(now.year(), now.month(), now.day(), 0, 0, 0);
    DateTime tomorrowBase = DateTime(now.year(), now.month(), now.day() + 1, 0, 0, 0);
    
    DateTime closestTime = DateTime(2099, 12, 31, 23, 59, 59); // Far future
    uint8_t closestIndex = 0;
    
    // Check today's schedules
    for (uint8_t i = 0; i < scheduleCount; i++) {
        if (!schedules[i].enabled) continue;
        
        DateTime scheduleTime = getScheduleDateTime(schedules[i], todayBase);
        
        if (scheduleTime > now && scheduleTime < closestTime) {
            closestTime = scheduleTime;
            closestIndex = i;
        }
    }
    
    // If no schedule found for today, check tomorrow
    if (closestTime.year() == 2099) {
        for (uint8_t i = 0; i < scheduleCount; i++) {
            if (!schedules[i].enabled) continue;
            
            DateTime scheduleTime = getScheduleDateTime(schedules[i], tomorrowBase);
            
            if (scheduleTime < closestTime) {
                closestTime = scheduleTime;
                closestIndex = i;
            }
        }
    }
    
    nextScheduledTime = closestTime;
    nextScheduleIndex = closestIndex;
}

/**
 * Get DateTime from schedule and reference date
 */
DateTime FeedingSchedule::getScheduleDateTime(const ScheduledFeeding& schedule, const DateTime& referenceDate) {
    return DateTime(referenceDate.year(), referenceDate.month(), referenceDate.day(),
                   schedule.hour, schedule.minute, schedule.second);
}

/**
 * Main processing method - NON-BLOCKING
 */
void FeedingSchedule::processSchedules(const DateTime& currentTime) {
    if (!scheduleEnabled || scheduleCount == 0 || !schedules || !modules || !modules->hasFeedingController()) {
        return;
    }
    
    // Skip if feeding is already in progress
    if (feedingInProgress) {
        // Check if feeding completed - we need access to the motor to check this
        // For now, let's trust the external feeding state management
        return;
    }
    
    // Check for missed feedings (power loss recovery)
    recoverMissedFeedings(currentTime);
    
    // Check if it's time for next scheduled feeding
    if (isTimeForFeeding(currentTime, schedules[nextScheduleIndex])) {
        executeFeeding(schedules[nextScheduleIndex]);
        calculateNextFeeding(); // Recalculate next feeding
    }
    
    // Update next scheduled time for web interface
    updateNextScheduledTime(currentTime);
}

/**
 * Check if current time matches scheduled feeding time
 */
bool FeedingSchedule::isTimeForFeeding(const DateTime& currentTime, const ScheduledFeeding& schedule) {
    if (!schedule.enabled) return false;
    
    DateTime scheduleTime = getScheduleDateTime(schedule, 
        DateTime(currentTime.year(), currentTime.month(), currentTime.day(), 0, 0, 0));
    
    // Check if we're within 1 minute of scheduled time
    long timeDiff = (long)(currentTime.unixtime() - scheduleTime.unixtime());
    if (timeDiff < 0) timeDiff = -timeDiff; // Manual abs() to avoid ambiguity
    return (timeDiff <= 60); // 60 seconds tolerance
}

/**
 * Check if a feeding was missed (for recovery)
 */
bool FeedingSchedule::isFeedingMissed(const DateTime& currentTime, const ScheduledFeeding& schedule) {
    if (!schedule.enabled) return false;
    
    DateTime scheduleTime = getScheduleDateTime(schedule,
        DateTime(currentTime.year(), currentTime.month(), currentTime.day(), 0, 0, 0));
    
    // If schedule time is in the past and beyond tolerance
    if (scheduleTime < currentTime) {
        long minutesPast = (currentTime.unixtime() - scheduleTime.unixtime()) / 60;
        return (minutesPast > 1 && minutesPast <= (toleranceMinutes));
    }
    
    return false;
}

/**
 * Execute a scheduled feeding
 */
void FeedingSchedule::executeFeeding(const ScheduledFeeding& schedule) {
    Console::printlnR("FeedingSchedule: Executing scheduled feeding - " + 
                     String(schedule.portions) + " portions at " + 
                     String(schedule.hour) + ":" + 
                     String(schedule.minute, DEC));
    
    if (strlen(schedule.description) > 0) {
        Console::printlnR("Description: " + String(schedule.description));
    }
    
    // Start feeding via controller
    if (modules->getFeedingController()->dispenseFoodAsync(schedule.portions)) {
        feedingInProgress = true;
    } else {
        Console::printlnR(F("FeedingSchedule: ERROR - Failed to start feeding"));
    }
    
    // Record this feeding time
    DateTime feedingTime = DateTime(); // Current time
    lastCompletedFeeding = feedingTime;
    saveLastFeedingToNVRAM(feedingTime);
}

/**
 * Recover missed feedings after power loss
 */
void FeedingSchedule::recoverMissedFeedings(const DateTime& currentTime) {
    // Look back from last completed feeding to find missed schedules
    DateTime searchStart = lastCompletedFeeding;
    DateTime searchEnd = currentTime;
    
    // Limit search to maximum recovery hours
    DateTime maxLookback = DateTime(currentTime.unixtime() - (maxRecoveryHours * 3600));
    if (searchStart < maxLookback) {
        searchStart = maxLookback;
    }
    
    // Check each day between searchStart and searchEnd
    DateTime checkDate = searchStart;
    
    while (checkDate < searchEnd) {
        for (uint8_t i = 0; i < scheduleCount; i++) {
            if (!schedules[i].enabled) continue;
            
            DateTime scheduleTime = getScheduleDateTime(schedules[i], checkDate);
            
            // Skip if this schedule is after current time
            if (scheduleTime >= currentTime) continue;
            
            // Skip if this schedule is before or at last completed feeding
            if (scheduleTime <= lastCompletedFeeding) continue;
            
            // Check if this feeding was missed and within tolerance
            long minutesPast = (currentTime.unixtime() - scheduleTime.unixtime()) / 60;
            if (minutesPast > 1 && minutesPast <= toleranceMinutes) {
                Console::printlnR("FeedingSchedule: RECOVERY - Missed feeding detected: " +
                                 formatTime(scheduleTime) + " (" + String(minutesPast) + " minutes ago)");
                
                executeFeeding(schedules[i]);
                return; // Execute one recovery feeding at a time
            }
        }
        
        // Move to next day
        checkDate = DateTime(checkDate.unixtime() + 86400); // +24 hours
    }
}

/**
 * Enable/disable schedule system
 */
void FeedingSchedule::enableSchedule(bool enabled) {
    scheduleEnabled = enabled;
    Console::printlnR("FeedingSchedule: System " + String(enabled ? "ENABLED" : "DISABLED"));
    
    if (enabled) {
        calculateNextFeeding();
        printNextFeeding();
    }
}

/**
 * Enable/disable specific schedule
 */
void FeedingSchedule::enableScheduleAtIndex(uint8_t index, bool enabled) {
    if (index >= scheduleCount || !schedules) {
        Console::printlnR(F("FeedingSchedule: ERROR - Invalid schedule index"));
        return;
    }
    
    schedules[index].enabled = enabled;
    Console::printlnR("Schedule " + String(index) + " " + String(enabled ? "ENABLED" : "DISABLED"));
    
    // Save to NVRAM
    saveSchedulesToNVRAM();
    calculateNextFeeding();
}

/**
 * Print schedule status
 */
void FeedingSchedule::printScheduleStatus() {
    Console::printlnR(F("\n=== FEEDING SCHEDULE STATUS ==="));
    Console::printlnR("System Status: " + String(scheduleEnabled ? "ENABLED" : "DISABLED"));
    Console::printlnR("Active Schedules: " + String(scheduleCount));
    Console::printlnR("Feeding In Progress: " + String(feedingInProgress ? "YES" : "NO"));
    Console::printlnR("Tolerance: " + String(toleranceMinutes) + " minutes");
    Console::printlnR("Max Recovery: " + String(maxRecoveryHours) + " hours");
    Console::printlnR("NVRAM Status: " + String(persistenceInitialized ? "OK" : "ERROR"));
    
    printLastFeeding();
    printNextFeeding();
}

/**
 * Print all schedules
 */
void FeedingSchedule::printScheduleList() {
    Console::printlnR(F("\n=== FEEDING SCHEDULES ==="));
    
    if (scheduleCount == 0 || !schedules) {
        Console::printlnR(F("No schedules configured"));
        return;
    }
    
    for (uint8_t i = 0; i < scheduleCount; i++) {
        String status = schedules[i].enabled ? "ON " : "OFF";
        String timeStr = String(schedules[i].hour, DEC) + ":" + 
                        String(schedules[i].minute < 10 ? "0" : "") + String(schedules[i].minute, DEC);
        
        if (schedules[i].second > 0) {
            timeStr += ":" + String(schedules[i].second < 10 ? "0" : "") + String(schedules[i].second, DEC);
        }
        
        Console::printlnR(String(i) + ": [" + status + "] " + timeStr + 
                         " - " + String(schedules[i].portions) + " portions" +
                         (strlen(schedules[i].description) > 0 ? " (" + String(schedules[i].description) + ")" : ""));
    }
}

/**
 * Print next feeding information
 */
void FeedingSchedule::printNextFeeding() {
    if (!scheduleEnabled || scheduleCount == 0) {
        Console::printlnR(F("Next Feeding: DISABLED"));
        return;
    }
    
    if (nextScheduledTime.year() == 2099) {
        Console::printlnR(F("Next Feeding: No active schedules"));
        return;
    }
    
    Console::printlnR("Next Feeding: " + formatTime(nextScheduledTime) + 
                     " (" + String(schedules[nextScheduleIndex].portions) + " portions)");
}

/**
 * Print last feeding information
 */
void FeedingSchedule::printLastFeeding() {
    Console::printlnR("Last Feeding: " + formatTime(lastCompletedFeeding));
}

/**
 * Record manual feeding (updates last feeding time)
 */
void FeedingSchedule::recordManualFeeding(const DateTime& feedingTime) {
    lastCompletedFeeding = feedingTime;
    saveLastFeedingToNVRAM(feedingTime);
    Console::printlnR("FeedingSchedule: Manual feeding recorded: " + formatTime(feedingTime));
}

/**
 * Configuration methods
 */
void FeedingSchedule::setTolerance(uint16_t toleranceMinutes) {
    this->toleranceMinutes = toleranceMinutes;
    Console::printlnR("FeedingSchedule: Tolerance set to " + String(toleranceMinutes) + " minutes");
}

void FeedingSchedule::setMaxRecoveryHours(uint16_t maxHours) {
    this->maxRecoveryHours = maxHours;
    Console::printlnR("FeedingSchedule: Max recovery set to " + String(maxHours) + " hours");
}

/**
 * Getter methods
 */
bool FeedingSchedule::isScheduleEnabled() { return scheduleEnabled; }
bool FeedingSchedule::isScheduleEnabled(uint8_t index) {
    if (index >= scheduleCount || !schedules) return false;
    return schedules[index].enabled;
}
DateTime FeedingSchedule::getNextScheduledTime() {
    // Return cached value if recently calculated
    return nextScheduledTime;
}

/**
 * Calculate and update the next scheduled feeding time
 */
void FeedingSchedule::updateNextScheduledTime(const DateTime& currentTime) {
    DateTime nextFeeding(2099, 1, 1, 0, 0, 0); // Far future date for comparison
    
    // Check all enabled schedules for the next one
    for (int i = 0; i < scheduleCount; i++) {
        if (!schedules[i].enabled) continue;
        
        // Calculate next occurrence for this schedule
        DateTime candidateTime(currentTime.year(), currentTime.month(), currentTime.day(), 
                              schedules[i].hour, schedules[i].minute, schedules[i].second);
        
        // If time already passed today, move to tomorrow
        if (candidateTime <= currentTime) {
            candidateTime = candidateTime + TimeSpan(1, 0, 0, 0); // Add 1 day
        }
        
        // Keep the earliest next feeding
        if (candidateTime < nextFeeding) {
            nextFeeding = candidateTime;
        }
    }
    
    // Update internal state
    if (nextFeeding.year() >= 2099) {
        nextScheduledTime = DateTime(2000, 1, 1, 0, 0, 0); // No active schedules
    } else {
        nextScheduledTime = nextFeeding;
    }
}
DateTime FeedingSchedule::getLastCompletedFeeding() { return lastCompletedFeeding; }
uint8_t FeedingSchedule::getScheduleCount() { return scheduleCount; }
uint16_t FeedingSchedule::getTolerance() { return toleranceMinutes; }
uint16_t FeedingSchedule::getMaxRecoveryHours() { return maxRecoveryHours; }

ScheduledFeeding FeedingSchedule::getSchedule(uint8_t index) {
    if (index >= scheduleCount || !schedules) {
        ScheduledFeeding empty = {0, 0, 0, 0, false, ""};
        return empty;
    }
    return schedules[index];
}

/**
 * Utility methods
 */
String FeedingSchedule::formatTime(const DateTime& dt) {
    if (dt.year() == 2000) {
        return "Never";
    }
    
    return String(dt.day()) + "/" + String(dt.month()) + "/" + String(dt.year()) + " " +
           String(dt.hour()) + ":" + 
           String(dt.minute() < 10 ? "0" : "") + String(dt.minute()) + ":" +
           String(dt.second() < 10 ? "0" : "") + String(dt.second());
}

String FeedingSchedule::formatSchedule(const ScheduledFeeding& schedule) {
    return String(schedule.hour) + ":" + 
           String(schedule.minute < 10 ? "0" : "") + String(schedule.minute) +
           (schedule.second > 0 ? ":" + String(schedule.second < 10 ? "0" : "") + String(schedule.second) : "");
}

/**
 * Diagnostics and testing
 */
void FeedingSchedule::printDiagnostics() {
    Console::printlnR(F("\n=== FEEDING SCHEDULE DIAGNOSTICS ==="));
    Console::printlnR("Memory - schedules pointer: " + String((unsigned long)schedules, HEX));
    Console::printlnR("Memory - feedingController pointer: " + String((unsigned long)(modules ? modules->getFeedingController() : nullptr), HEX));
    Console::printlnR("State - scheduleEnabled: " + String(scheduleEnabled));
    Console::printlnR("State - feedingInProgress: " + String(feedingInProgress));
    Console::printlnR("State - persistenceInitialized: " + String(persistenceInitialized));
    Console::printlnR("Next Schedule Index: " + String(nextScheduleIndex));
    
    // NVRAM diagnostic
    if (persistenceInitialized) {
        uint32_t storedTime = preferences.getUInt("last_feeding", 0);
        Console::printlnR("NVRAM stored timestamp: " + String(storedTime));
    }
}

void FeedingSchedule::testScheduleCalculation() {
    Console::printlnR(F("\n=== SCHEDULE CALCULATION TEST ==="));
    calculateNextFeeding();
    printNextFeeding();
    
    Console::printlnR(F("Testing with current time..."));
    DateTime testTime = DateTime(); // Current time
    
    for (uint8_t i = 0; i < scheduleCount; i++) {
        if (!schedules) break;
        
        bool timeMatch = isTimeForFeeding(testTime, schedules[i]);
        bool missed = isFeedingMissed(testTime, schedules[i]);
        
        Console::printlnR("Schedule " + String(i) + ": Time match=" + String(timeMatch) + 
                         ", Missed=" + String(missed));
    }
}

/**
 * Edit existing schedule
 */
bool FeedingSchedule::editSchedule(uint8_t index, uint8_t hour, uint8_t minute, uint8_t second, uint8_t portions, const char* description) {
    if (index >= scheduleCount) {
        Console::printlnR(F("FeedingSchedule: ERROR - Invalid schedule index"));
        return false;
    }
    
    if (hour > 23 || minute > 59 || second > 59 || portions < 1 || portions > 10) {
        Console::printlnR(F("FeedingSchedule: ERROR - Invalid schedule parameters"));
        return false;
    }
    
    // Update schedule in internal storage
    scheduleStorage[index].hour = hour;
    scheduleStorage[index].minute = minute;
    scheduleStorage[index].second = second;
    scheduleStorage[index].portions = portions;
    strncpy(scheduleStorage[index].description, description, 49);
    scheduleStorage[index].description[49] = '\0'; // Ensure null termination
    
    // Save to NVRAM
    saveSchedulesToNVRAM();
    calculateNextFeeding();
    
    Console::printlnR("FeedingSchedule: Schedule " + String(index) + " updated");
    return true;
}

/**
 * Remove schedule at index
 */
bool FeedingSchedule::removeSchedule(uint8_t index) {
    if (index >= scheduleCount) {
        Console::printlnR(F("FeedingSchedule: ERROR - Invalid schedule index"));
        return false;
    }
    
    // Shift all schedules after this one down by one position
    for (uint8_t i = index; i < scheduleCount - 1; i++) {
        scheduleStorage[i] = scheduleStorage[i + 1];
    }
    
    scheduleCount--;
    
    // Clear the last slot
    memset(&scheduleStorage[scheduleCount], 0, sizeof(ScheduledFeeding));
    
    // Save to NVRAM
    saveSchedulesToNVRAM();
    calculateNextFeeding();
    
    Console::printlnR("FeedingSchedule: Schedule " + String(index) + " removed");
    return true;
}

/**
 * Load schedules from NVRAM
 * If NVRAM is empty, initialize with DEFAULT_FEEDING_SCHEDULE
 */
void FeedingSchedule::loadSchedulesFromNVRAM() {
    if (!persistenceInitialized) {
        Console::printlnR(F("FeedingSchedule: WARNING - Cannot load from NVRAM, not initialized"));
        return;
    }
    
    // Check if schedules exist in NVRAM
    uint8_t storedCount = preferences.getUChar("sched_count", 255);
    
    if (storedCount == 255) {
        // No schedules in NVRAM - initialize with defaults
        Console::printlnR(F("FeedingSchedule: No schedules in NVRAM - initializing with defaults"));
        initializeDefaultSchedules();
        return;
    }
    
    if (storedCount > 10) { // MAX_SCHEDULED_FEEDINGS
        Console::printlnR(F("FeedingSchedule: WARNING - Invalid schedule count in NVRAM, initializing with defaults"));
        initializeDefaultSchedules();
        return;
    }
    
    Console::printlnR("FeedingSchedule: Loading " + String(storedCount) + " schedules from NVRAM");
    
    // Load each schedule from NVRAM
    scheduleCount = 0;
    for (uint8_t i = 0; i < storedCount; i++) {
        String prefix = "s" + String(i) + "_";
        
        scheduleStorage[i].hour = preferences.getUChar((prefix + "h").c_str(), 0);
        scheduleStorage[i].minute = preferences.getUChar((prefix + "m").c_str(), 0);
        scheduleStorage[i].second = preferences.getUChar((prefix + "s").c_str(), 0);
        scheduleStorage[i].portions = preferences.getUChar((prefix + "p").c_str(), 1);
        scheduleStorage[i].enabled = preferences.getBool((prefix + "en").c_str(), true);
        
        String desc = preferences.getString((prefix + "desc").c_str(), "");
        strncpy(scheduleStorage[i].description, desc.c_str(), 49);
        scheduleStorage[i].description[49] = '\0';
        
        Console::printlnR("  Loaded: " + String(scheduleStorage[i].hour) + ":" + 
                         String(scheduleStorage[i].minute) + " - " + 
                         String(scheduleStorage[i].portions) + " portions");
        
        scheduleCount++;
    }
    
    calculateNextFeeding();
    Console::printlnR("FeedingSchedule: Successfully loaded " + String(scheduleCount) + " schedules from NVRAM");
}

/**
 * Save schedules to NVRAM
 */
void FeedingSchedule::saveSchedulesToNVRAM() {
    if (!persistenceInitialized) {
        Console::printlnR(F("FeedingSchedule: WARNING - Cannot save to NVRAM, not initialized"));
        return;
    }
    
    Console::printlnR("FeedingSchedule: Saving " + String(scheduleCount) + " schedules to NVRAM");
    
    // Save schedule count
    preferences.putUChar("sched_count", scheduleCount);
    
    // Save each schedule
    for (uint8_t i = 0; i < scheduleCount; i++) {
        String prefix = "s" + String(i) + "_";
        
        preferences.putUChar((prefix + "h").c_str(), scheduleStorage[i].hour);
        preferences.putUChar((prefix + "m").c_str(), scheduleStorage[i].minute);
        preferences.putUChar((prefix + "s").c_str(), scheduleStorage[i].second);
        preferences.putUChar((prefix + "p").c_str(), scheduleStorage[i].portions);
        preferences.putBool((prefix + "en").c_str(), scheduleStorage[i].enabled);
        preferences.putString((prefix + "desc").c_str(), String(scheduleStorage[i].description));
    }
    
    Console::printlnR(F("FeedingSchedule: Schedules saved to NVRAM successfully"));
}

/**
 * Initialize with DEFAULT_FEEDING_SCHEDULE from config.h
 * This is only called on first boot or NVRAM reset
 */
void FeedingSchedule::initializeDefaultSchedules() {
    Console::printlnR(F("FeedingSchedule: Initializing default schedules"));
    
    scheduleCount = 0;
    
    // Copy DEFAULT_FEEDING_SCHEDULE to internal storage
    for (uint8_t i = 0; i < DEFAULT_SCHEDULE_COUNT && i < 10; i++) {
        scheduleStorage[i] = DEFAULT_FEEDING_SCHEDULE[i];
        scheduleCount++;
    }
    
    // Save to NVRAM
    saveSchedulesToNVRAM();
    calculateNextFeeding();
    
    Console::printlnR("FeedingSchedule: Initialized with " + String(scheduleCount) + " default schedules");
    printScheduleList();
}
