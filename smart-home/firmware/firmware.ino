/*
 * BLE Service & TinyML Voice Controller - Main Entry Point
 * Smart Home Appliance Controller
 *
 * Hardware: Arduino Nano 33 BLE Sense (Rev2 compatible)
 * Architecture: Modular firmware with separated concerns
 */

#include <ArduinoBLE.h>
#include "ble_serial.h"
#include "command_parser.h"
#include "appliance_controller.h"
#include "safety_manager.h"
#include "tinyml/voice_recognizer.h"

// ============================================================================
// GLOBAL INSTANCES
// ============================================================================

BLESerial bleSerial;
CommandParser cmdParser;
ApplianceController applianceCtrl;
SafetyManager safetyMgr;
VoiceRecognizer voiceRecognizer;

// Forward declarations
void handleSerialCommands();
void processCommand(ParsedCommand& cmd);
void setupBLE();
void handleBLECommands();

// ============================================================================
// SAFETY MANAGER CALLBACKS
// ============================================================================

void onSafetyShutoff(int applianceId) {
  Serial.print("[SafetyManager] Forcing Appliance ");
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
  Serial.begin(115200);
  unsigned long serialStart = millis();
  while (!Serial && millis() - serialStart < 3000);

  Serial.println();
  Serial.println("========================================================");
  Serial.println("    SMART HOME CONTROLLER - TINYML VOICE TEST MODE      ");
  Serial.println("========================================================");

  // Wire safety callbacks before begin()
  safetyMgr.setShutoffCallback(onSafetyShutoff);
  safetyMgr.setStateCallback(checkApplianceState);

  // Initialize subsystems
  safetyMgr.begin();
  applianceCtrl.setSafetyCheckCallback(checkApplianceSafe);
  applianceCtrl.begin();
  applianceCtrl.setBLESerial(&bleSerial);
  cmdParser.begin();
  setupBLE();

  // Initialize TinyML voice recognition
  Serial.println("[TinyML] Initializing Voice Recognition pipeline...");
  if (!voiceRecognizer.begin()) {
    Serial.println("[TinyML] WARNING: Voice recognition failed to initialize!");
    Serial.println("  -> BLE control is still available");
  } else {
    voiceRecognizer.setDebug(false); // Live hearing feedback is automatic; press 'd' for raw stream
    Serial.println("[TinyML] Voice Recognition: ACTIVE & LISTENING (Strict Gating)");
    Serial.println("[TinyML] Model: trial33 (16 kHz, 2.0s window, INT8)");
    Serial.println("[TinyML] Target Commands: \"fan on\", \"fan off\", \"light on\", \"light off\"");
    Serial.println("[TinyML] Gating: Confidence >= 78%, Margin >= 20%, VAD Peak >= 1500, Lockout: 2.0s");
    Serial.println("[Help] '+' / '-' tune threshold, 't' params, 'd' stream scores, 'h' menu");
  }

  Serial.println("========================================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Update BLE serial TX/RX buffers
  bleSerial.update();

  // Check BLE connection status
  static bool wasConnected = false;
  bool isConnected = BLE.connected();

  if (isConnected && !wasConnected) {
    Serial.println("[BLE] Central device connected!");
  } else if (!isConnected && wasConnected) {
    Serial.println("[BLE] Central device disconnected. Re-advertising...");
  }
  wasConnected = isConnected;

  if (isConnected) {
    handleBLECommands();
  } else {
    BLE.advertise();
  }

  // Handle USB Serial inputs from computer (test commands)
  handleSerialCommands();

  // ---- TinyML Voice Recognition ----
  voiceRecognizer.update();

  if (voiceRecognizer.hasCommand()) {
    ParsedCommand voiceCmd = voiceRecognizer.getCommand();
    Serial.print("\n>>> [VOICE RECOGNIZED] ");
    Serial.println(voiceCmd.rawCommand);

    // Feed through the same safety-validated pipeline as BLE commands
    processCommand(voiceCmd);
  }

  // Run safety checks periodically
  safetyMgr.loop();
}

// ============================================================================
// BLE SETUP
// ============================================================================

void setupBLE() {
  if (!BLE.begin()) {
    Serial.println("[BLE] ERROR: BLE initialization failed!");
    while (1);
  }

  BLE.setLocalName("SmartHomeCtrl");
  BLE.setDeviceName("SmartHomeCtrl");

  bleSerial.begin();
  BLE.advertise();

  Serial.println("[BLE] Advertising as 'SmartHomeCtrl'");
}

// ============================================================================
// BLE COMMAND HANDLING
// ============================================================================

void handleBLECommands() {
  while (bleSerial.available()) {
    char c = bleSerial.read();
    ParsedCommand* cmd = cmdParser.processChar(c);

    if (cmd != nullptr) {
      Serial.print("[BLE CMD] ");
      Serial.println(cmd->rawCommand);
      processCommand(*cmd);
      cmdParser.reset();
    }
  }
}

// ============================================================================
// USB SERIAL COMMAND HANDLING (Interactive Diagnostics)
// ============================================================================

void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') continue;

    if (c == 'd' || c == 'D') {
      bool newState = !voiceRecognizer.isDebug();
      voiceRecognizer.setDebug(newState);
      Serial.print("\n[Diagnostic] Live score streaming: ");
      Serial.println(newState ? "ENABLED" : "MUTED");
    } else if (c == '1') {
      Serial.println("\n[Manual Test] Actuating Appliance 1 (Light) ON");
      ParsedCommand cmd;
      cmd.type = CMD_ON; cmd.applianceId = 1; cmd.intensity = 100; cmd.valid = true;
      snprintf(cmd.rawCommand, sizeof(cmd.rawCommand), "SERIAL:light_on");
      processCommand(cmd);
    } else if (c == '0') {
      Serial.println("\n[Manual Test] Actuating Appliance 1 (Light) OFF");
      ParsedCommand cmd;
      cmd.type = CMD_OFF; cmd.applianceId = 1; cmd.intensity = 0; cmd.valid = true;
      snprintf(cmd.rawCommand, sizeof(cmd.rawCommand), "SERIAL:light_off");
      processCommand(cmd);
    } else if (c == '2') {
      Serial.println("\n[Manual Test] Actuating Appliance 2 (Fan) ON");
      ParsedCommand cmd;
      cmd.type = CMD_ON; cmd.applianceId = 2; cmd.intensity = 100; cmd.valid = true;
      snprintf(cmd.rawCommand, sizeof(cmd.rawCommand), "SERIAL:fan_on");
      processCommand(cmd);
    } else if (c == 'f' || c == 'F') {
      Serial.println("\n[Manual Test] Actuating Appliance 2 (Fan) OFF");
      ParsedCommand cmd;
      cmd.type = CMD_OFF; cmd.applianceId = 2; cmd.intensity = 0; cmd.valid = true;
      snprintf(cmd.rawCommand, sizeof(cmd.rawCommand), "SERIAL:fan_off");
      processCommand(cmd);
    } else if (c == 's' || c == 'S') {
      Serial.println("\n--- Current Appliance Status ---");
      for (int i = 1; i <= MAX_APPLIANCES; i++) {
        Appliance* a = applianceCtrl.getAppliance(i);
        if (a) {
          Serial.print("  Appliance "); Serial.print(i);
          Serial.print(" (Pin "); Serial.print(a->pin); Serial.print("): ");
          Serial.print(a->isOn ? "ON" : "OFF");
          Serial.print(" | Intensity: "); Serial.print(a->intensity); Serial.println("%");
        }
      }
      Serial.println("--------------------------------\n");
    } else if (c == '+') {
      voiceRecognizer.adjustThreshold(0.02f);
    } else if (c == '-') {
      voiceRecognizer.adjustThreshold(-0.02f);
    } else if (c == 't' || c == 'T') {
      voiceRecognizer.printThresholds();
    } else if (c == 'h' || c == 'H' || c == '?') {
      Serial.println("\n================ Serial Test Commands ================");
      Serial.println("  'd' - Toggle live model inference score streaming");
      Serial.println("  '+' - Increase voice threshold by +2% (stricter)");
      Serial.println("  '-' - Decrease voice threshold by -2% (more sensitive)");
      Serial.println("  't' - Print current TinyML gating parameters");
      Serial.println("  '1' - Turn Appliance 1 (Light) ON");
      Serial.println("  '0' - Turn Appliance 1 (Light) OFF");
      Serial.println("  '2' - Turn Appliance 2 (Fan) ON");
      Serial.println("  'f' - Turn Appliance 2 (Fan) OFF");
      Serial.println("  's' - Print all appliances status");
      Serial.println("  'h' - Show this menu");
      Serial.println("======================================================\n");
    }
  }
}

// ============================================================================
// COMMAND PROCESSING PIPELINE
// ============================================================================

void processCommand(ParsedCommand& cmd) {
  // Stage 1: Validate command
  if (!safetyMgr.validateCommand(cmd)) {
    Serial.println("  -> [REJECTED] Safety validation failed");
    bleSerial.println("ERROR: Command rejected by safety manager");
    return;
  }

  // Stage 2: Trigger safety manager emergency if needed
  if (cmd.type == CMD_EMERGENCY_STOP) {
    safetyMgr.triggerEmergency();
  }

  // Stage 3: Execute command
  bool success = applianceCtrl.execute(cmd);

  // Stage 4: Send response & feedback
  if (success) {
    bleSerial.println("OK");
    Serial.print("  -> [SUCCESS] Actuated: ");
    Serial.println(cmd.rawCommand);
  } else {
    bleSerial.println("ERROR: Execution failed");
    Serial.println("  -> [FAILED] Execution rejected or hardware lock");
  }
}
