/*
 * Appliance Controller Module
 * Manages GPIO outputs and appliance states
 * 
 * Hardware Abstraction Layer for appliance control
 */

#ifndef APPLIANCE_CONTROLLER_H
#define APPLIANCE_CONTROLLER_H

#include <Arduino.h>
#include "ble_serial.h"
#include "command_parser.h"

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// GPIO pin assignments for Arduino Nano 33 BLE Sense
// PWM-capable pins: 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A2, A3, A4, A5
const int PIN_APPLIANCE_1 = 2;   // Light / Relay 1
const int PIN_APPLIANCE_2 = 3;   // Fan / Relay 2
const int PIN_APPLIANCE_3 = 4;   // Heater / Relay 3
const int PIN_APPLIANCE_4 = 5;   // Pump / Relay 4

const int MAX_APPLIANCES = 4;

// ============================================================================
// APPLIANCE STATE STRUCTURE
// ============================================================================

struct Appliance {
  int id;
  int pin;
  bool isOn;
  int intensity;      // 0-100%
  unsigned long onTime; // Timestamp when turned on
  unsigned long totalOnTime; // Cumulative on-time in milliseconds
  
  Appliance() : id(0), pin(0), isOn(false), intensity(0), 
                onTime(0), totalOnTime(0) {}
};

// ============================================================================
// APPLIANCE CONTROLLER CLASS
// ============================================================================

typedef bool (*SafetyCheckCallback)(int applianceId);

class ApplianceController {
private:
  Appliance appliances[MAX_APPLIANCES];
  BLESerial* bleSerial;
  SafetyCheckCallback safetyCheck = nullptr;
  
  // PWM output helper
  void setOutput(int pin, int intensity) {
    if (intensity == 0) {
      digitalWrite(pin, LOW);
    } else if (intensity == 100) {
      digitalWrite(pin, HIGH);
    } else {
      // Map 0-100 to 0-255 for PWM
      analogWrite(pin, map(intensity, 0, 100, 0, 255));
    }
  }
  
public:
  ApplianceController() : bleSerial(nullptr) {
    // Initialize appliance array
    for (int i = 0; i < MAX_APPLIANCES; i++) {
      appliances[i].id = i + 1;
      appliances[i].isOn = false;
      appliances[i].intensity = 0;
      appliances[i].onTime = 0;
      appliances[i].totalOnTime = 0;
    }
    appliances[0].pin = PIN_APPLIANCE_1;
    appliances[1].pin = PIN_APPLIANCE_2;
    appliances[2].pin = PIN_APPLIANCE_3;
    appliances[3].pin = PIN_APPLIANCE_4;
  }
  
  // Initialize controller
  void begin() {
    // Configure GPIO pins
    for (int i = 0; i < MAX_APPLIANCES; i++) {
      pinMode(appliances[i].pin, OUTPUT);
      digitalWrite(appliances[i].pin, LOW);
    }
    
    Serial.println("ApplianceController: Initialized");
    Serial.print("  - Appliance 1: Pin ");
    Serial.println(PIN_APPLIANCE_1);
    Serial.print("  - Appliance 2: Pin ");
    Serial.println(PIN_APPLIANCE_2);
    Serial.print("  - Appliance 3: Pin ");
    Serial.println(PIN_APPLIANCE_3);
    Serial.print("  - Appliance 4: Pin ");
    Serial.println(PIN_APPLIANCE_4);
  }
  
  // Set BLE serial reference for feedback
  void setBLESerial(BLESerial* serial) {
    bleSerial = serial;
  }
  
  void setSafetyCheckCallback(SafetyCheckCallback cb) {
    safetyCheck = cb;
  }
  
  // Execute a parsed command
  bool execute(ParsedCommand& cmd) {
    switch (cmd.type) {
      case CMD_ON:
        return turnOn(cmd.applianceId);
        
      case CMD_OFF:
        return turnOff(cmd.applianceId);
        
      case CMD_DIM:
        return setIntensity(cmd.applianceId, cmd.intensity);
        
      case CMD_TOGGLE:
        return toggle(cmd.applianceId);
        
      case CMD_STATUS:
        sendStatus();
        return true;
        
      case CMD_HELP:
        sendHelp();
        return true;
        
      case CMD_SCENE:
        return activateScene(cmd.applianceId);
        
      case CMD_EMERGENCY_STOP:
        emergencyStop();
        return true;
        
      default:
        Serial.println("ApplianceController: Unknown command type");
        return false;
    }
  }
  
  // Turn on appliance
  bool turnOn(int applianceId) {
    if (!validateApplianceId(applianceId)) return false;
    
    // ENFORCE SAFETY AT THE ACTUATOR LEVEL
    if (safetyCheck && !safetyCheck(applianceId)) {
      if (bleSerial) {
        bleSerial->print("ERROR: Safety lock on Appliance ");
        bleSerial->println(applianceId);
      }
      return false;
    }
    
    Appliance& app = appliances[applianceId - 1];
    app.isOn = true;
    app.intensity = 100;
    app.onTime = millis();
    digitalWrite(app.pin, HIGH);
    
    Serial.print("ApplianceController: Appliance ");
    Serial.print(applianceId);
    Serial.println(" turned ON");
    
    return true;
  }
  
  // Turn off appliance
  bool turnOff(int applianceId) {
    if (!validateApplianceId(applianceId)) return false;
    
    Appliance& app = appliances[applianceId - 1];
    
    // Update total on-time
    if (app.isOn) {
      app.totalOnTime += (millis() - app.onTime);
    }
    
    app.isOn = false;
    app.intensity = 0;
    app.onTime = 0;
    digitalWrite(app.pin, LOW);
    
    Serial.print("ApplianceController: Appliance ");
    Serial.print(applianceId);
    Serial.println(" turned OFF");
    
    return true;
  }
  
  // Set intensity (dimming)
  bool setIntensity(int applianceId, int intensity) {
    if (!validateApplianceId(applianceId)) return false;
    if (intensity < 0 || intensity > 100) return false;
    
    // ENFORCE SAFETY AT THE ACTUATOR LEVEL
    if (intensity > 0 && safetyCheck && !safetyCheck(applianceId)) {
      if (bleSerial) {
        bleSerial->print("ERROR: Safety lock on Appliance ");
        bleSerial->println(applianceId);
      }
      return false;
    }
    
    Appliance& app = appliances[applianceId - 1];
    app.intensity = intensity;
    
    if (intensity > 0) {
      if (!app.isOn) {
        app.onTime = millis();
      }
      app.isOn = true;
      setOutput(app.pin, intensity);
    } else {
      if (app.isOn) {
        app.totalOnTime += (millis() - app.onTime);
      }
      app.isOn = false;
      digitalWrite(app.pin, LOW);
    }
    
    Serial.print("ApplianceController: Appliance ");
    Serial.print(applianceId);
    Serial.print(" set to ");
    Serial.print(intensity);
    Serial.println("%");
    
    return true;
  }
  
  // Toggle appliance state
  bool toggle(int applianceId) {
    if (!validateApplianceId(applianceId)) return false;
    
    Appliance& app = appliances[applianceId - 1];
    
    if (app.isOn) {
      return turnOff(applianceId);
    } else {
      return turnOn(applianceId);
    }
  }
  
  // Activate predefined scene
  bool activateScene(int sceneId) {
    Serial.print("ApplianceController: Activating scene ");
    Serial.println(sceneId);
    
    switch (sceneId) {
      case 1: // "Away" - All off
        turnOff(1); turnOff(2); turnOff(3); turnOff(4);
        break;
        
      case 2: // "Home" - Lights on
        turnOn(1); turnOn(2); turnOff(3); turnOff(4);
        break;
        
      case 3: // "Night" - Dim lights
        setIntensity(1, 30); setIntensity(2, 20); turnOff(3); turnOff(4);
        break;
        
      case 4: // "Party" - All on
        turnOn(1); turnOn(2); turnOn(3); turnOn(4);
        break;
        
      default:
        Serial.println("ApplianceController: Unknown scene");
        return false;
    }
    
    return true;
  }
  
  // Emergency stop - all appliances off immediately
  void emergencyStop() {
    Serial.println("ApplianceController: EMERGENCY STOP");
    
    for (int i = 1; i <= MAX_APPLIANCES; i++) {
      turnOff(i);
    }
    
    if (bleSerial) {
      bleSerial->println("EMERGENCY STOP ACTIVATED");
    }
  }
  
  // Send status over BLE
  void sendStatus() {
    if (!bleSerial) return;
    
    bleSerial->println("=== APPLIANCE STATUS ===");
    
    for (int i = 0; i < MAX_APPLIANCES; i++) {
      Appliance& app = appliances[i];
      bleSerial->print("Appliance ");
      bleSerial->print(app.id);
      bleSerial->print(": ");
      bleSerial->print(app.isOn ? "ON" : "OFF");
      bleSerial->print(" (");
      bleSerial->print(app.intensity);
      bleSerial->print("%)");
      
      // Show runtime if on
      if (app.isOn) {
        unsigned long currentRun = (millis() - app.onTime) / 1000;
        bleSerial->print(" [");
        bleSerial->print(currentRun);
        bleSerial->print("s]");
      }
      
      bleSerial->println();
    }
    
    bleSerial->println("========================");
  }
  
  // Send help message
  void sendHelp() {
    if (!bleSerial) return;
    
    bleSerial->println("=== AVAILABLE COMMANDS ===");
    bleSerial->println("ON<n>      - Turn ON appliance n (1-4)");
    bleSerial->println("OFF<n>     - Turn OFF appliance n (1-4)");
    bleSerial->println("TOGGLE<n>  - Toggle appliance n (1-4)");
    bleSerial->println("DIM<n,p>   - Set appliance n to p% (e.g., DIM:1,50)");
    bleSerial->println("SCENE<n>   - Activate scene n (1-4)");
    bleSerial->println("STATUS     - Show all appliance states");
    bleSerial->println("HELP       - Show this help message");
    bleSerial->println("EMERGENCY  - Emergency stop (all off)");
    bleSerial->println("==========================");
  }
  
  // Get appliance state (for external access)
  Appliance* getAppliance(int id) {
    if (!validateApplianceId(id)) return nullptr;
    return &appliances[id - 1];
  }
  
private:
  // Validate appliance ID
  bool validateApplianceId(int id) {
    if (id < 1 || id > MAX_APPLIANCES) {
      Serial.print("ApplianceController: Invalid appliance ID: ");
      Serial.println(id);
      return false;
    }
    return true;
  }
};

#endif // APPLIANCE_CONTROLLER_H