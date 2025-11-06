#ifndef CONSOLE_MANAGER_H
#define CONSOLE_MANAGER_H

#include <Arduino.h>

/**
 * ConsoleManager Class
 * 
 * Manages console logging and output control for the Fish Feeder system.
 * This class provides dual logging system with standard and response outputs.
 */
class ConsoleManager {
public:
    
    // Logging control
    static bool isLoggingEnabled;
    
    // Standard logging methods (respect logging state)
    static void logPrint(const String& message);
    static void logPrint(const __FlashStringHelper* message);
    static void logPrintln(const String& message);
    static void logPrintln(const __FlashStringHelper* message);
    
    // Custom Console methods for external use
    class Console {
    public:
        // Standard output (respect logging state)
        static void print(const String& message);
        static void print(const __FlashStringHelper* message);
        static void println(const String& message);
        static void println(const __FlashStringHelper* message);
        
        // Response output (always print, regardless of logging state)
        static void printR(const String& message);
        static void printR(const __FlashStringHelper* message);
        static void printlnR(const String& message);
        static void printlnR(const __FlashStringHelper* message);
    };
};

// Convenient alias for external use
using Console = ConsoleManager::Console;

#endif // CONSOLE_MANAGER_H