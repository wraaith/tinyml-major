/*
 * Command Mapper Module
 * Maps voice classification labels to smart-home ParsedCommands.
 *
 * Current Model (trial33) Labels:
 *   0: "fan off"    → Appliance 2 (Fan) OFF
 *   1: "fan on"     → Appliance 2 (Fan) ON
 *   2: "light off"  → Appliance 1 (Light) OFF
 *   3: "light on"   → Appliance 1 (Light) ON
 *   4: "noise"      → Ignored
 *   5: "unknown"    → Ignored
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
    static bool mapLabelToCommand(int labelIndex, float confidence, ParsedCommand& outCmd, float threshold = VM_CONFIDENCE_THRESHOLD) {
        outCmd = ParsedCommand();   // Reset

        // Reject if below dynamic or default confidence threshold
        if (confidence < threshold) {
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
            case VM_LABEL_UNKNOWN:
                // Background noise or unclassified words are not actionable
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
        Serial.println("=== Voice -> Command Mapping ===");
        Serial.println("  \"light on\"   -> Appliance 1 ON");
        Serial.println("  \"light off\"  -> Appliance 1 OFF");
        Serial.println("  \"fan on\"     -> Appliance 2 ON");
        Serial.println("  \"fan off\"    -> Appliance 2 OFF");
        Serial.println("  \"noise\"      -> (ignored)");
        Serial.println("  \"unknown\"    -> (ignored)");
        Serial.print("  Confidence threshold: ");
        Serial.print(VM_CONFIDENCE_THRESHOLD * 100);
        Serial.println("%");
        Serial.print("  Cooldown lockout:     ");
        Serial.print(VM_INFERENCE_COOLDOWN_MS);
        Serial.println(" ms");
        Serial.println("================================");
    }
};

#endif // COMMAND_MAPPER_H
