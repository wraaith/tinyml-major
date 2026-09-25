/*
* BLE Service - Main Entry Point
* Smart Home Appliance Controller
*
* Hardware: Arduino Nano 33 BLE Sense Rev2
* Architecture: Modular firmware with separated concerns
*/

#include <ArduinoBLE.h>
#include "ble_serial.h"
#include "command_parser.h"
#include "appliance_controller.h"
#include "safety_manager.h"

// ============================================================================
// GLOBAL INSTANCES
// ============================================================================

BLESerial bleSerial;
CommandParser cmdParser;
ApplianceController applianceCtrl;
SafetyManager safetyMgr;

// ============================================================================
// SAFETY MANAGER CALLBACKS
//
// FIX: connects SafetyManager to ApplianceController so cooldown triggers
// and emergency stop actually turn appliances off instead of only logging.
//
// NOTE: appliance_controller.h wasn't available when this was written -
// adapt `applianceCtrl.turnOff(id)` and `applianceCtrl.isOn(id)` below to
// match your actual ApplianceController method names if they differ.
// ============================================================================

void onSafetyShutoff(int applianceId) {
  Serial.print("SafetyManager -> ApplianceController: forcing appliance ");
  Serial.print(applianceId);
  Serial.println(" OFF");
  applianceCtrl.turnOff(applianceId);
}

bool checkApplianceState(int applianceId) {
  Appliance* app = applianceCtrl.getAppliance(applianceId);
  return app ? app->isOn : false;
}

bool checkApplianceSafe(int applianceId) {
  return safetyMgr.isSafeToActuate(applianceId);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Initialize debug serial (timeout after 3s for headless operation)
  Serial.begin(9600);
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 3000);

  Serial.println("=== Smart Home Controller Starting ===");

  // FIX: wire safety callbacks before begin() so checks are live from boot
  safetyMgr.setShutoffCallback(onSafetyShutoff);
  safetyMgr.setStateCallback(checkApplianceState);

  // Initialize subsystems
  safetyMgr.begin();
  applianceCtrl.setSafetyCheckCallback(checkApplianceSafe);
  applianceCtrl.begin();
  applianceCtrl.setBLESerial(&bleSerial);
  cmdParser.begin();
  setupBLE();

  Serial.println("=== System Ready ===");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // FIX: was bleSerial.poll() - update() also drains the TX ring buffer
  // without blocking, so SafetyManager and CommandParser never get starved
  bleSerial.update();

  // Check BLE connection status
  if (BLE.connected()) {
    handleBLECommands();
  } else {
    // Re-advertise when disconnected
    BLE.advertise();
  }

  // Run safety checks periodically
  safetyMgr.loop();
}

// ============================================================================
// BLE SETUP
// ============================================================================

void setupBLE() {
  if (!BLE.begin()) {
    Serial.println("ERROR: BLE initialization failed!");
    while (1);
  }

  // Device identification
  BLE.setLocalName("SmartHomeCtrl");
  BLE.setDeviceName("SmartHomeCtrl");

  // Start BLE serial service (Nordic UART)
  bleSerial.begin();

  // Start advertising
  BLE.advertise();

  Serial.println("BLE Service: Advertising as 'SmartHomeCtrl'");
}

// ============================================================================
// BLE COMMAND HANDLING
// ============================================================================

void handleBLECommands() {
  // Process all available BLE data
  while (bleSerial.available()) {
    char c = bleSerial.read();

    // Feed character to command parser
    ParsedCommand* cmd = cmdParser.processChar(c);

    if (cmd != nullptr) {
      // Command complete - process it
      processCommand(*cmd);

      // Reset parser for next command
      cmdParser.reset();
    }
  }
}

// ============================================================================
// COMMAND PROCESSING PIPELINE
// ============================================================================

void processCommand(ParsedCommand& cmd) {
  Serial.print("Processing: ");
  Serial.println(cmd.rawCommand);

  // Stage 1: Validate command
  if (!safetyMgr.validateCommand(cmd)) {
    Serial.println("REJECTED: Safety validation failed");
    bleSerial.println("ERROR: Command rejected by safety manager");
    return;
  }

  // Stage 2: Trigger safety manager emergency if needed
  if (cmd.type == CMD_EMERGENCY_STOP) {
    safetyMgr.triggerEmergency();
  }

  // Stage 3: Execute command
  bool success = applianceCtrl.execute(cmd);

  // Stage 4: Send response
  if (success) {
    bleSerial.println("OK");
    Serial.println("EXECUTED");
  } else {
    bleSerial.println("ERROR: Execution failed");
    Serial.println("FAILED");
  }
} // FIX: this closing brace was missing in the original file
