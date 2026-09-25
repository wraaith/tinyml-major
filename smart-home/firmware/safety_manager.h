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

const int MAX_TOTAL_CURRENT_MA = 5000;
const unsigned long MAX_RUNTIME_MS = 3600000UL; // 1 hour
const unsigned long COOLDOWN_MS = 300000UL;     // 5 minutes
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
// SAFETY MANAGER CALLBACK TYPES
// ============================================================================

// Called by SafetyManager to actively force an appliance off
// (e.g. on max-runtime cooldown trigger or emergency stop)
typedef void (*ShutoffCallback)(int applianceId);

// Called by SafetyManager to ask "is this appliance currently ON?"
typedef bool (*ApplianceStateCallback)(int applianceId);

// ============================================================================
// SAFETY MANAGER CLASS
// ============================================================================

class SafetyManager {
private:
  SafetyState currentState;
  unsigned long lastCheckTime;
  bool emergencyActive;

  ShutoffCallback shutoffCallback = nullptr;
  ApplianceStateCallback stateCallback = nullptr;

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
    if (applianceId >= 1 && applianceId <= 4) {
      return CURRENT_DRAW_MA[applianceId - 1];
    }
    return 0;
  }

  // FIX: previously hardcoded to return 0, which meant checkTotalLoad()
  // never actually triggered. Now sums the draw of every appliance the
  // state callback reports as ON.
  int getTotalCurrentDraw() {
    int total = 0;
    if (stateCallback) {
      for (int id = 1; id <= 4; id++) {
        if (stateCallback(id)) {
          total += getCurrentDraw(id);
        }
      }
    }
    return total;
  }

public:
  SafetyManager() : currentState(SAFETY_OK), emergencyActive(false) {
    lastCheckTime = 0;
  }

  // Register the callback SafetyManager calls to force an appliance off
  void setShutoffCallback(ShutoffCallback cb) { shutoffCallback = cb; }

  // Register the callback SafetyManager calls to query appliance ON/OFF state
  void setStateCallback(ApplianceStateCallback cb) { stateCallback = cb; }

  // Initialize safety manager
  void begin() {
    Serial.println("SafetyManager: Initialized");
    Serial.print(" - Max total current: ");
    Serial.print(MAX_TOTAL_CURRENT_MA);
    Serial.println(" mA");
    Serial.print(" - Max runtime: ");
    Serial.print(MAX_RUNTIME_MS / 1000);
    Serial.println(" seconds");
  }

  // Main safety loop - call regularly
  void loop() {
    unsigned long now = millis();

    if (now - lastCheckTime >= 1000) {
      lastCheckTime = now;
      performSafetyChecks();
    }

    updateCooldowns();
  }

  // Validate command before execution
  bool validateCommand(ParsedCommand& cmd) {
    if (cmd.type == CMD_EMERGENCY_STOP) {
      return true;
    }

    if (emergencyActive && cmd.type != CMD_STATUS && cmd.type != CMD_HELP) {
      Serial.println("SafetyManager: Emergency active - commands blocked");
      return false;
    }

    if (cmd.applianceId >= 1 && cmd.applianceId <= 4) {
      // Cooldown enforcement - blocks re-activation until cooldown clears
      if (applianceSafety[cmd.applianceId - 1].isCooldown) {
        Serial.print("SafetyManager: Appliance ");
        Serial.print(cmd.applianceId);
        Serial.println(" in cooldown");
        return false;
      }

      if (checkOverheat(cmd.applianceId)) {
        Serial.print("SafetyManager: Appliance ");
        Serial.print(cmd.applianceId);
        Serial.println(" overheated");
        return false;
      }
    }

    if (cmd.type == CMD_ON || cmd.type == CMD_DIM) {
      if (!checkTotalLoad(cmd.applianceId)) {
        Serial.println("SafetyManager: Total load exceeded");
        return false;
      }
    }

    return true;
  }

  bool checkOverheat(int applianceId) {
    // In production: read temperature sensor (DS18B20, thermistor, etc.)
    return false;
  }

  // Preventive check - blocks turning ON a NEW appliance if it would
  // exceed total load. Does not need to force anything off, since it
  // runs before the appliance is ever activated.
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

  bool isSafeToActuate(int id) {
    if (id < 1 || id > 4) return false;
    if (emergencyActive) return false;
    if (applianceSafety[id - 1].isCooldown) return false;
    
    // If it's already on, actuating it again doesn't add new load
    if (stateCallback && stateCallback(id)) return true;
    return checkTotalLoad(id);
  }

  SafetyState getState() { return currentState; }
  bool isEmergencyActive() { return emergencyActive; }

  void clearEmergency() {
    emergencyActive = false;
    currentState = SAFETY_OK;
    Serial.println("SafetyManager: Emergency cleared");
  }

  void triggerEmergency() {
    emergencyActive = true;
    currentState = SAFETY_EMERGENCY;
    Serial.println("SafetyManager: EMERGENCY TRIGGERED");

    // FIX: previously only logged - now actively forces every appliance off
    if (shutoffCallback) {
      for (int id = 1; id <= 4; id++) {
        shutoffCallback(id);
      }
    }
  }

private:
  // FIX: runtime is now tracked here directly via stateCallback, instead of
  // relying on recordActivation()/updateRuntime(), which nothing called.
  void performSafetyChecks() {
    unsigned long now = millis();

    // Proactive Load Shedding
    int currentDraw = getTotalCurrentDraw();
    if (currentDraw > MAX_TOTAL_CURRENT_MA) {
      Serial.print("SafetyManager: CRITICAL LOAD EXCEEDED (");
      Serial.print(currentDraw);
      Serial.println(" mA)");
      
      currentState = SAFETY_CRITICAL;
      
      while (currentDraw > MAX_TOTAL_CURRENT_MA) {
        int highestDraw = -1;
        int targetApplianceId = -1;
        
        for (int i = 1; i <= 4; i++) {
          if (stateCallback && stateCallback(i)) {
            int draw = getCurrentDraw(i);
            if (draw > highestDraw) {
              highestDraw = draw;
              targetApplianceId = i;
            }
          }
        }
        
        if (targetApplianceId != -1) {
          Serial.print("SafetyManager: Shedding Appliance ");
          Serial.println(targetApplianceId);
          if (shutoffCallback) shutoffCallback(targetApplianceId);
          currentDraw -= highestDraw; // Simulate reduction to prevent infinite loop
        } else {
          triggerEmergency();
          break;
        }
      }
    }

    for (int i = 0; i < 4; i++) {
      int id = i + 1;
      bool isOn = stateCallback ? stateCallback(id) : false;

      if (isOn) {
        if (applianceSafety[i].lastActivation == 0) {
          applianceSafety[i].lastActivation = now;
        }
        applianceSafety[i].continuousRunTime = now - applianceSafety[i].lastActivation;
      } else {
        applianceSafety[i].lastActivation = 0;
        applianceSafety[i].continuousRunTime = 0;
      }

      if (applianceSafety[i].continuousRunTime > MAX_RUNTIME_MS) {
        Serial.print("SafetyManager: Appliance ");
        Serial.print(id);
        Serial.println(" exceeded max runtime - initiating cooldown & shutoff");

        applianceSafety[i].isCooldown = true;
        applianceSafety[i].cooldownEnd = now + COOLDOWN_MS;
        applianceSafety[i].continuousRunTime = 0;
        applianceSafety[i].lastActivation = 0;

        // FIX: actively force the appliance off instead of just logging
        if (shutoffCallback) {
          shutoffCallback(id);
        }
      }
    }

    currentState = emergencyActive ? SAFETY_EMERGENCY : SAFETY_OK;
  }

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

  // NOTE: kept for backward compatibility / manual instrumentation. No
  // longer required for correct operation now that performSafetyChecks()
  // polls appliance state directly via the stateCallback.
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

  void updateRuntime(int applianceId, bool isOn) {
    if (applianceId < 1 || applianceId > 4) return;

    if (isOn) {
      applianceSafety[applianceId - 1].continuousRunTime =
        millis() - applianceSafety[applianceId - 1].lastActivation;
    }
  }

  friend class ApplianceController;
};

#endif // SAFETY_MANAGER_H
