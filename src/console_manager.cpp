#include "console_manager.h"

// Static member initialization
bool ConsoleManager::isLoggingEnabled = true;

/**
 * Logging control functions - only print if logging is enabled
 */
void ConsoleManager::logPrint(const String& message) {
    if (isLoggingEnabled) {
        Serial.print(message);
    }
}

void ConsoleManager::logPrint(const __FlashStringHelper* message) {
    if (isLoggingEnabled) {
        Serial.print(message);
    }
}

void ConsoleManager::logPrintln(const String& message) {
    if (isLoggingEnabled) {
        Serial.println(message);
    }
}

void ConsoleManager::logPrintln(const __FlashStringHelper* message) {
    if (isLoggingEnabled) {
        Serial.println(message);
    }
}

// ============================================================================
// CONSOLE CLASS IMPLEMENTATIONS
// ============================================================================

/**
 * Console::print methods (respect logging state)
 */
void ConsoleManager::Console::print(const String& message) {
    if (ConsoleManager::isLoggingEnabled) {
        Serial.print(message);
    }
}

void ConsoleManager::Console::print(const __FlashStringHelper* message) {
    if (ConsoleManager::isLoggingEnabled) {
        Serial.print(message);
    }
}

void ConsoleManager::Console::println(const String& message) {
    if (ConsoleManager::isLoggingEnabled) {
        Serial.println(message);
    }
}

void ConsoleManager::Console::println(const __FlashStringHelper* message) {
    if (ConsoleManager::isLoggingEnabled) {
        Serial.println(message);
    }
}

/**
 * Console::printR methods (always print - Response mode)
 */
void ConsoleManager::Console::printR(const String& message) {
    Serial.print(message);
}

void ConsoleManager::Console::printR(const __FlashStringHelper* message) {
    Serial.print(message);
}

void ConsoleManager::Console::printlnR(const String& message) {
    Serial.println(message);
}

void ConsoleManager::Console::printlnR(const __FlashStringHelper* message) {
    Serial.println(message);
}