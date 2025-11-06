#include "rtc_module.h"

RTCModule::RTCModule() {
  // Construtor vazio
}

bool RTCModule::testDS3231Communication() {
  Serial.println("Testando comunicação específica com DS3231...");
  
  // Try to read seconds register (address 0x00)
  Wire.beginTransmission(0x68);
  Wire.write(0x00); // Seconds register
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.print(F("Error sending command: "));
    Serial.println(error);
    return false;
  }
  
  // Try to read data
  Wire.requestFrom(0x68, 1);
  if (Wire.available()) {
    byte seconds = Wire.read();
    Serial.print(F("✓ Data read from DS3231: 0x"));
    Serial.println(seconds, HEX);
    return true;
  } else {
    Serial.println(F("✗ No data received from DS3231"));
    return false;
  }
}

bool RTCModule::begin() {
  // Initialize Wire (I2C)
  Wire.begin();
  
  Serial.println(F("=== DS3231 RTC Module ==="));
  Serial.println(F("ESP32 - Correct connections:"));
  Serial.println(F("VCC → 3.3V or 5V"));
  Serial.println(F("GND → GND"));
  Serial.println(F("SDA → GPIO 21"));
  Serial.println(F("SCL → GPIO 22"));
  Serial.println(F("========================"));
  
  // Scan I2C devices first
  scanI2C();
  
  Serial.println(F("Attempting to initialize DS3231 RTC..."));
  
  // Try different initialization methods
  bool rtcOK = false;
  
  // Method 1: Standard initialization
  if (rtc.begin()) {
    Serial.println(F("✓ RTC initialized successfully (standard method)"));
    rtcOK = true;
  } else {
    Serial.println(F("✗ Standard initialization failed"));
    
    // Method 2: Try restarting Wire and try again
    Serial.println(F("Attempting to reinitialize I2C..."));
    Wire.end();
    delay(100);
    Wire.begin();
    delay(100);
    
    if (rtc.begin()) {
      Serial.println(F("✓ RTC initialized after I2C reinitialization"));
      rtcOK = true;
    } else {
      Serial.println(F("✗ Failed after I2C reinitialization"));
      
      // Method 3: Check direct I2C communication
      Serial.println(F("Testing direct I2C communication..."));
      Wire.beginTransmission(0x68); // DS3231 address
      byte error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.println(F("✓ DS3231 responds at address 0x68"));
        Serial.println(F("Problem may be in RTClib library"));
        
        // Try one last time
        delay(500);
        if (rtc.begin()) {
          Serial.println(F("✓ RTC finally initialized!"));
          rtcOK = true;
        }
      } else {
        Serial.print(F("✗ I2C Error: "));
        Serial.println(error);
      }
    }
  }
  
  if (!rtcOK) {
    Serial.println();
    Serial.println(F("=== COMPLETE DIAGNOSIS ==="));
    Serial.println(F("ERROR: Could not initialize DS3231!"));
    Serial.println();
    Serial.println("Verification checklist (ESP32):");
    Serial.println("□ VCC conectado ao 5V (ou 3.3V)");
    Serial.println("□ GND conectado ao GND");
    Serial.println(F("□ SDA connected to GPIO 21"));
    Serial.println(F("□ SCL connected to GPIO 22"));
    Serial.println("□ Módulo DS3231 não danificado");
    Serial.println("□ Alimentação adequada (3.3V-5V)");
    return false;
  }
  
  // Verificar se o RTC perdeu energia e resetar se necessário
  if (rtc.lostPower()) {
    Serial.println("RTC perdeu energia, configurando com data/hora de compilação!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  Serial.println(F("✓ RTC module initialized successfully!"));
  
  // Mostrar temperatura do RTC
  Serial.print("Temperatura do RTC: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" °C");
  
  return true;
}

void RTCModule::scanI2C() {
  Serial.println(F("Scanning I2C devices..."));
  byte error, address;
  int nDevices = 0;
  
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print(F("I2C device found at address 0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.print(address, HEX);
      if (address == 0x68) {
        Serial.print(F(" (DS3231 RTC)"));
        Serial.println();
        if (testDS3231Communication()) {
          Serial.println(F("  → DS3231 communication OK"));
        } else {
          Serial.println(F("  → DS3231 communication problem"));
        }
      } else {
        Serial.println();
      }
      nDevices++;
    }
    else if (error == 4) {
      Serial.print(F("Unknown error at address 0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.println(address, HEX);
    }
  }
  
  if (nDevices == 0) {
    Serial.println(F("No I2C devices found!"));
    Serial.println();
    Serial.println("CONNECTION CHECKLIST (ESP32):");
    Serial.println("1. VCC → 5V (vermelho)");
    Serial.println(F("2. GND → GND (black)"));
    Serial.println(F("3. SDA → GPIO 21 (data)"));
    Serial.println(F("4. SCL → GPIO 22 (clock)"));
    Serial.println();
    Serial.println(F("TIPS:"));
    Serial.println(F("- Check if wires are properly connected"));
    Serial.println(F("- Test with different cables if possible"));
    Serial.println(F("- Check if module is receiving power"));
  } else {
    Serial.print(F("Total devices found: "));
    Serial.println(nDevices);
  }
  Serial.println();
}

DateTime RTCModule::now() {
  return rtc.now();
}

bool RTCModule::lostPower() {
  return rtc.lostPower();
}

void RTCModule::adjust(const DateTime& dt) {
  rtc.adjust(dt);
}

float RTCModule::getTemperature() {
  return rtc.getTemperature();
}

void RTCModule::showAdjustInstructions() {
  Serial.println();
  Serial.println(F("=== TIME ADJUSTMENT ==="));
  Serial.println(F("Type 'SET' to adjust current time"));
  Serial.println(F("Format: SET DD/MM/YYYY HH:MM:SS"));
  Serial.println(F("Example: SET 27/10/2025 13:30:00"));
  Serial.println(F("======================="));
}

bool RTCModule::processCommand(String comando) {
  comando.trim();
  comando.toUpperCase();
  
  if (comando.startsWith("SET ")) {
    String dataHora = comando.substring(4);
    dataHora.trim();
    
    // Expected format: DD/MM/YYYY HH:MM:SS
    int dia, mes, ano, hora, minuto, segundo;
    
    if (sscanf(dataHora.c_str(), "%d/%d/%d %d:%d:%d", &dia, &mes, &ano, &hora, &minuto, &segundo) == 6) {
      
      // Validate data
      if (dia >= 1 && dia <= 31 && mes >= 1 && mes <= 12 && ano >= 2000 && ano <= 2099 &&
          hora >= 0 && hora <= 23 && minuto >= 0 && minuto <= 59 && segundo >= 0 && segundo <= 59) {
        
        // Adjust the RTC
        rtc.adjust(DateTime(ano, mes, dia, hora, minuto, segundo));
        
        Serial.println(F("✓ Time adjusted successfully!"));
        Serial.print(F("New date/time: "));
        Serial.print(dia);
        Serial.print(F("/"));
        Serial.print(mes);
        Serial.print(F("/"));
        Serial.print(ano);
        Serial.print(F(" "));
        Serial.print(hora);
        Serial.print(F(":"));
        Serial.print(minuto);
        Serial.print(F(":"));
        Serial.println(segundo);
        
        return true;
      } else {
        Serial.println(F("✗ Error: Invalid values!"));
      }
    } else {
      Serial.println(F("✗ Error: Invalid format!"));
      Serial.println(F("Use: SET DD/MM/YYYY HH:MM:SS"));
    }
  } else {
    Serial.println(F("✗ Command not recognized!"));
    Serial.println(F("Use: SET DD/MM/YYYY HH:MM:SS"));
  }
  
  return false;
}

void RTCModule::printDateTime() {
  DateTime now = rtc.now();
  
  // Display date and time in DD/MM/YYYY HH:MM:SS format
  Serial.print(F("Date/Time: "));
  if (now.day() < 10) Serial.print("0");
  Serial.print(now.day(), DEC);
  Serial.print('/');
  if (now.month() < 10) Serial.print("0");
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.year(), DEC);
  Serial.print(" ");
  if (now.hour() < 10) Serial.print("0");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  if (now.minute() < 10) Serial.print("0");
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  if (now.second() < 10) Serial.print("0");
  Serial.print(now.second(), DEC);
  
  // Show day of week
  const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  Serial.print(F(" - "));
  Serial.print(dayNames[now.dayOfTheWeek()]);
  
  // Show Unix timestamp
  Serial.print(F(" (Unix: "));
  Serial.print(now.unixtime());
  Serial.println(F(")"));
}

bool RTCModule::isWorking() {
  return rtc.begin();
}