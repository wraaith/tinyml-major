/*
 * Safety Manager Module
 * Validates commands and enforces safety constraints
 * 
 * Features:
 * - Command validation
 * - Load monitoring
 * - Overheat protection
 * - Emergency stop
 * - Interlock management
 */

#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <Arduino.h>
#include "command_parser.h"

// ============================================================================
// SAFETY CONFIGURATION
// ============================================================================

// Maximum total current (in mA) - adjust based on your relay/power supply
const int MAX_TOTAL_CURRENT_MA = 5000;

// Maximum continuous runtime before auto-shutoff (in milliseconds)
const unsigned long MAX_RUNTIME_MS = 3600000UL; // 1 hour

// Cooldown period after max runtime (in milliseconds)
const unsigned long COOLDOWN_MS = 300000UL; // 5 minutes

// Simulated current draw per appliance (mA) - adjust for your loads
const int CURRENT_DRAW_MA[4] = {500, 800, 1500, 1000}; // App 1-4

// ============================================================================
// SAFETY STATE ENUMERATION
// ============================================================================

enum SafetyState {
  SAFETY_OK,
  SAFETY_WARNING,
  SAFETY_CRITICAL,
  SAFETY_EMERGENCY
};

// ============================================================================
// SAFETY MANAGER CLASS
// ============================================================================

class SafetyManager {
private:
  SafetyState currentState;
  unsigned long lastCheckTime;
  bool emergencyActive;
  
  // Per-appliance safety tracking
  struct ApplianceSafety {
    unsigned long lastActivation;
    unsigned long continuousRunTime;
    bool isCooldown;
    unsigned long cooldownEnd;
    
    ApplianceSafety() : lastActivation(0), continuousRunTime(0), 
                        isCooldown(false), cooldownEnd(0) {}
  };
  
  ApplianceSafety applianceSafety[4];
  
  // Simulated current monitoring (replace with actual sensor if available)
  int getCurrentDraw(int applianceId) {
    // In production, read from current sensor (ACS712, INA219, etc.)
    if (applianceId >= 1 && applianceId <= 4) {
      return CURRENT_DRAW_MA[applianceId - 1];
    }
    return 0;
  }
  
  int getTotalCurrentDraw() {
    // This would sum actual readings from all active appliances
    // For now, return simulated value
    return 0;
  }
  
public:
  SafetyManager() : currentState(SAFETY_OK), emergencyActive(false) {
    lastCheckTime = 0;
  }
  
  // Initialize safety manager
  void begin() {
    Serial.println("SafetyManager: Initialized");
    Serial.print("  - Max total current: ");
    Serial.print(MAX_TOTAL_CURRENT_MA);
    Serial.println(" mA");
    Serial.print("  - Max runtime: ");
    Serial.print(MAX_RUNTIME_MS / 1000);
    Serial.println(" seconds");
  }
  
  // Main safety loop - call regularly
  void loop() {
    unsigned long now = millis();
    
    // Check every 1 second
    if (now - lastCheckTime >= 1000) {
      lastCheckTime = now;
      performSafetyChecks();
    }
    
    // Update cooldown timers
    updateCooldowns();
  }
  
  // Validate command before execution
  bool validateCommand(ParsedCommand& cmd) {
    // Emergency commands always allowed
    if (cmd.type == CMD_EMERGENCY_STOP) {
      return true;
    }
    
    // Check if emergency is active
    if (emergencyActive && cmd.type != CMD_STATUS && cmd.type != CMD_HELP) {
      Serial.println("SafetyManager: Emergency active - commands blocked");
      return false;
    }
    
    // Validate appliance-specific commands
    if (cmd.applianceId >= 1 && cmd.applianceId <= 4) {
      // Check if appliance is in cooldown
      if (applianceSafety[cmd.applianceId - 1].isCooldown) {
        Serial.print("SafetyManager: Appliance ");
        Serial.print(cmd.applianceId);
        Serial.println(" in cooldown");
        return false;
      }
      
      // Check for overheat (simulated - add temperature sensor in production)
      if (checkOverheat(cmd.applianceId)) {
        Serial.print("SafetyManager: Appliance ");
        Serial.print(cmd.applianceId);
        Serial.println(" overheated");
        return false;
      }
    }
    
    // Check total load for ON commands
    if (cmd.type == CMD_ON || cmd.type == CMD_DIM) {
      if (!checkTotalLoad(cmd.applianceId)) {
        Serial.println("SafetyManager: Total load exceeded");
        return false;
      }
    }
    
    return true;
  }
  
  // Check if appliance is overheated (simulated)
  bool checkOverheat(int applianceId) {
    // In production: read temperature sensor (DS18B20, thermistor, etc.)
    // For now, always return false (no overheat)
    return false;
  }
  
  // Check if turning on this appliance would exceed total load
  bool checkTotalLoad(int applianceId) {
    int totalCurrent = getTotalCurrentDraw();
    int newCurrent = totalCurrent + getCurrentDraw(applianceId);
    
    if (newCurrent > MAX_TOTAL_CURRENT_MA) {
      Serial.print("SafetyManager: Load would exceed limit (");
      Serial.print(newCurrent);
      Serial.print(" > ");
      Serial.print(MAX_TOTAL_CURRENT_MA);
      Serial.println(" mA)");
      return false;
    }
    
    return true;
  }
  
  // Get current safety state
  SafetyState getState() {
    return currentState;
  }
  
  // Check if emergency stop is active
  bool isEmergencyActive() {
    return emergencyActive;
  }
  
  // Clear emergency state (requires physical button or special command)
  void clearEmergency() {
    emergencyActive = false;
    currentState = SAFETY_OK;
    Serial.println("SafetyManager: Emergency cleared");
  }
  
  // Trigger emergency stop
  void triggerEmergency() {
    emergencyActive = true;
    currentState = SAFETY_EMERGENCY;
    Serial.println("SafetyManager: EMERGENCY TRIGGERED");
  }
  
private:
  // Perform periodic safety checks
  void performSafetyChecks() {
    // Check for appliances exceeding max runtime
    for (int i = 0; i < 4; i++) {
      if (applianceSafety[i].continuousRunTime > MAX_RUNTIME_MS) {
        Serial.print("SafetyManager: Appliance ");
        Serial.print(i + 1);
        Serial.println(" exceeded max runtime - initiating cooldown");
        
        applianceSafety[i].isCooldown = true;
        applianceSafety[i].cooldownEnd = millis() + COOLDOWN_MS;
        
        // Signal appliance controller to turn off
        // (This would be done via callback or shared state in production)
      }
    }
    
    // Update safety state
    if (emergencyActive) {
      currentState = SAFETY_EMERGENCY;
    } else {
      currentState = SAFETY_OK;
    }
  }
  
  // Update cooldown timers
  void updateCooldowns() {
    unsigned long now = millis();
    
    for (int i = 0; i < 4; i++) {
      if (applianceSafety[i].isCooldown) {
        if (now >= applianceSafety[i].cooldownEnd) {
          applianceSafety[i].isCooldown = false;
          applianceSafety[i].continuousRunTime = 0;
          
          Serial.print("SafetyManager: Appliance ");
          Serial.print(i + 1);
          Serial.println(" cooldown complete");
        }
      }
    }
  }
  
  // Record appliance activation (call from appliance controller)
  void recordActivation(int applianceId, bool isOn) {
    if (applianceId < 1 || applianceId > 4) return;
    
    ApplianceSafety& safety = applianceSafety[applianceId - 1];
    
    if (isOn) {
      safety.lastActivation = millis();
      safety.continuousRunTime = 0;
    } else {
      safety.continuousRunTime = 0;
    }
  }
  
  // Update runtime tracking (call periodically from appliance controller)
  void updateRuntime(int applianceId, bool isOn) {
    if (applianceId < 1 || applianceId > 4) return;
    
    if (isOn) {
      applianceSafety[applianceId - 1].continuousRunTime = 
        millis() - applianceSafety[applianceId - 1].lastActivation;
    }
  }
  
  // Friend declaration for appliance controller access
  friend class ApplianceController;
};

#endif // SAFETY_MANAGER_H