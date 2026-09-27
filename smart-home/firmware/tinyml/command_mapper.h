/*
 * Command Mapper Module
 * Maps voice classification labels to smart-home ParsedCommands.
 *
 * The classifier produces labels like "fan_on", "light_off", etc.
 * This module translates those into the same ParsedCommand structs
 * that the BLE CommandParser produces, allowing seamless integration
 * with the existing ApplianceController pipeline.
 *
 * Appliance mapping:
 *   Appliance 1 (Pin 2) = Light   → "light_on" / "light_off"
 *   Appliance 2 (Pin 3) = Fan     → "fan_on"   / "fan_off"
 */

#ifndef COMMAND_MAPPER_H
#define COMMAND_MAPPER_H

#include <Arduino.h>
#include "../command_parser.h"   // For ParsedCommand & CommandType
#include "voice_model.h"

// ============================================================================
// VOICE-TO-COMMAND MAPPING
// ============================================================================

class CommandMapper {
public:
    // Convert a voice label index + confidence into a ParsedCommand.
    // Returns true if a valid, actionable command was produced.
    static bool mapLabelToCommand(int labelIndex, float confidence, ParsedCommand& outCmd) {
        outCmd = ParsedCommand();   // Reset

        // Reject low-confidence or noise
        if (confidence < VM_CONFIDENCE_THRESHOLD) {
            return false;
        }

        switch (labelIndex) {
            case VM_LABEL_LIGHT_ON:
                outCmd.type = CMD_ON;
                outCmd.applianceId = 1;       // Appliance 1 = Light
                outCmd.intensity = 100;
                outCmd.valid = true;
                snprintf(outCmd.rawCommand, sizeof(outCmd.rawCommand),
                         "VOICE:light_on (%.0f%%)", confidence * 100);
                break;

            case VM_LABEL_LIGHT_OFF:
                outCmd.type = CMD_OFF;
                outCmd.applianceId = 1;       // Appliance 1 = Light
                outCmd.intensity = 0;
                outCmd.valid = true;
                snprintf(outCmd.rawCommand, sizeof(outCmd.rawCommand),
                         "VOICE:light_off (%.0f%%)", confidence * 100);
                break;

            case VM_LABEL_FAN_ON:
                outCmd.type = CMD_ON;
                outCmd.applianceId = 2;       // Appliance 2 = Fan
                outCmd.intensity = 100;
                outCmd.valid = true;
                snprintf(outCmd.rawCommand, sizeof(outCmd.rawCommand),
                         "VOICE:fan_on (%.0f%%)", confidence * 100);
                break;

            case VM_LABEL_FAN_OFF:
                outCmd.type = CMD_OFF;
                outCmd.applianceId = 2;       // Appliance 2 = Fan
                outCmd.intensity = 0;
                outCmd.valid = true;
                snprintf(outCmd.rawCommand, sizeof(outCmd.rawCommand),
                         "VOICE:fan_off (%.0f%%)", confidence * 100);
                break;

            case VM_LABEL_NOISE:
                // Noise is not actionable
                return false;

            default:
                return false;
        }

        return true;
    }

    // Convenience: find the winning label from a classification result array
    static int getTopLabel(const float* scores, int count, float& outConfidence) {
        int   bestIdx  = -1;
        float bestConf = -1.0f;

        for (int i = 0; i < count; i++) {
            if (scores[i] > bestConf) {
                bestConf = scores[i];
                bestIdx  = i;
            }
        }

        outConfidence = bestConf;
        return bestIdx;
    }

    // Print the label-to-appliance mapping for debugging
    static void printMapping() {
        Serial.println("=== Voice → Command Mapping ===");
        Serial.println("  \"light_on\"   → Appliance 1 ON");
        Serial.println("  \"light_off\"  → Appliance 1 OFF");
        Serial.println("  \"fan_on\"     → Appliance 2 ON");
        Serial.println("  \"fan_off\"    → Appliance 2 OFF");
        Serial.println("  \"noise\"      → (ignored)");
        Serial.print("  Confidence threshold: ");
        Serial.print(VM_CONFIDENCE_THRESHOLD * 100);
        Serial.println("%");
        Serial.print("  Consecutive hits req: ");
        Serial.println(VM_CONSECUTIVE_HITS);
        Serial.println("================================");
    }
};

#endif // COMMAND_MAPPER_H
