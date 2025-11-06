#ifndef RTC_MODULE_H
#define RTC_MODULE_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

class RTCModule {
private:
  RTC_DS3231 rtc;
  
  // Função para testar comunicação I2C específica com DS3231
  bool testDS3231Communication();
  
public:
  // Constructor
  RTCModule();
  
  // RTC module initialization
  bool begin();
  
  // Diagnostic functions
  void scanI2C();
  
  // Get current date/time
  DateTime now();
  
  // Check if RTC lost power
  bool lostPower();
  
  // Adjust date/time
  void adjust(const DateTime& dt);
  
  // Get RTC temperature
  float getTemperature();
  
  // Process time adjustment command via Serial
  bool processCommand(String comando);
  
  // Show adjustment instructions
  void showAdjustInstructions();
  
  // Format and display complete date/time
  void printDateTime();
  
  // Validate if RTC is working
  bool isWorking();
};

#endif