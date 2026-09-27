/*
 * Voice Recognizer Module
 * Orchestrates continuous audio capture → Edge Impulse inference → command output.
 *
 * This is the top-level TinyML module. It:
 *   1. Captures audio via AudioCapture (PDM double-buffer)
 *   2. Feeds slices into run_classifier_continuous() (EI SDK)
 *   3. Applies confidence thresholding + consecutive-hit filtering
 *   4. Maps winning labels to ParsedCommands via CommandMapper
 *   5. Exposes ready commands to the main firmware loop
 *
 * Hardware: Arduino Nano 33 BLE Sense (PDM microphone)
 * Model:    INT8 quantized, EON-compiled, 5-class voice classifier
 */

#ifndef VOICE_RECOGNIZER_H
#define VOICE_RECOGNIZER_H

#include <Arduino.h>

// Edge Impulse SDK — must be included before our headers so macros are set
#define EIDSP_QUANTIZE_FILTERBANK   0
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW  VM_SLICES_PER_WINDOW

#include <tinyml_majar_2_inferencing.h>

#include "voice_model.h"
#include "audio_capture.h"
#include "feature_extraction.h"
#include "command_mapper.h"

// ============================================================================
// VOICE RECOGNIZER CLASS
// ============================================================================

class VoiceRecognizer {
private:
    AudioCapture    audioCapture;
    bool            initialized;
    bool            enabled;            // Can be toggled at runtime

    // Consecutive-hit tracking for debouncing
    int             lastLabelIndex;
    int             consecutiveCount;

    // Cooldown to avoid rapid re-triggering
    unsigned long   lastCommandTime;

    // Result accumulation — track initial fill period
    int             sliceCounter;

    // Latest ready command (set when all criteria are met)
    bool            commandReady;
    ParsedCommand   pendingCommand;

    // Debug flag
    bool            debugMode;

public:
    VoiceRecognizer()
        : initialized(false)
        , enabled(true)
        , lastLabelIndex(-1)
        , consecutiveCount(0)
        , lastCommandTime(0)
        , sliceCounter(-(VM_SLICES_PER_WINDOW))  // Let first N slices accumulate
        , commandReady(false)
        , debugMode(false)
    {}

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    bool begin() {
        Serial.println("VoiceRecognizer: Initializing...");

        // Print model info
        FeatureExtraction::printConfig();
        FeatureExtraction::printQuantizationInfo();
        CommandMapper::printMapping();

        // Initialize the EI continuous classifier
        run_classifier_init();

        // Start audio capture
        if (!audioCapture.begin(VM_SLICE_SIZE)) {
            Serial.println("VoiceRecognizer: FAILED — could not start audio capture");
            return false;
        }

        initialized = true;
        Serial.println("VoiceRecognizer: Ready — listening for voice commands");
        return true;
    }

    void end() {
        if (initialized) {
            audioCapture.end();
            initialized = false;
            Serial.println("VoiceRecognizer: Stopped");
        }
    }

    // ========================================================================
    // MAIN UPDATE — call this every loop() iteration
    // ========================================================================

    void update() {
        if (!initialized || !enabled) return;

        // Non-blocking: check if audio slice is ready
        if (!audioCapture.isSliceReady()) return;

        // Consume the slice
        audioCapture.consumeSlice();

        // Build EI signal from the ready audio buffer
        signal_t signal;
        signal.total_length = audioCapture.getSliceSize();
        signal.get_data     = &AudioCapture::getSignalData;

        // Run continuous classifier
        ei_impulse_result_t result = {0};
        EI_IMPULSE_ERROR err = run_classifier_continuous(&signal, &result, debugMode);

        if (err != EI_IMPULSE_OK) {
            Serial.print("VoiceRecognizer: Classifier error ");
            Serial.println(err);
            return;
        }

        sliceCounter++;

        // Only evaluate results after the initial window has been filled.
        // The EI SDK runs inference on every call once the window is full,
        // so we must process EVERY result after the fill period — not just
        // every N-th one — or we lose 75% of detections.
        if (sliceCounter >= 0) {
            processResult(result);
        }
    }

    // ========================================================================
    // COMMAND ACCESS — check after update()
    // ========================================================================

    bool hasCommand() const {
        return commandReady;
    }

    // Returns the pending command and clears it.
    ParsedCommand getCommand() {
        commandReady = false;
        return pendingCommand;
    }

    // ========================================================================
    // CONTROL
    // ========================================================================

    void setEnabled(bool en) {
        enabled = en;
        if (!en) {
            lastLabelIndex   = -1;
            consecutiveCount = 0;
        }
        Serial.print("VoiceRecognizer: ");
        Serial.println(en ? "Enabled" : "Disabled");
    }

    bool isEnabled()     const { return enabled; }
    bool isInitialized() const { return initialized; }

    void setDebug(bool dbg) { debugMode = dbg; }

private:
    // ========================================================================
    // RESULT PROCESSING
    // ========================================================================

    void processResult(const ei_impulse_result_t& result) {
        // Extract scores into a flat array
        float scores[VM_LABEL_COUNT];
        for (int i = 0; i < VM_LABEL_COUNT; i++) {
            scores[i] = result.classification[i].value;
        }

        // Find top label
        float confidence;
        int topLabel = CommandMapper::getTopLabel(scores, VM_LABEL_COUNT, confidence);

        // Debug print
        if (debugMode) {
            Serial.print("  [VR] DSP: "); Serial.print(result.timing.dsp);
            Serial.print(" ms, NN: ");     Serial.print(result.timing.classification);
            Serial.println(" ms");
            for (int i = 0; i < VM_LABEL_COUNT; i++) {
                Serial.print("    ");
                Serial.print(VM_LABELS[i]);
                Serial.print(": ");
                Serial.println(scores[i], 5);
            }
        }

        // Check noise rejection
        if (scores[VM_LABEL_NOISE] > VM_NOISE_REJECT_THRESHOLD) {
            lastLabelIndex   = VM_LABEL_NOISE;
            consecutiveCount = 0;
            return;
        }

        // Check confidence threshold
        if (topLabel < 0 || confidence < VM_CONFIDENCE_THRESHOLD) {
            consecutiveCount = 0;
            return;
        }

        // Ignore noise label even if top
        if (topLabel == VM_LABEL_NOISE) {
            consecutiveCount = 0;
            return;
        }

        // Consecutive-hit debounce
        if (topLabel == lastLabelIndex) {
            consecutiveCount++;
        } else {
            lastLabelIndex   = topLabel;
            consecutiveCount = 1;
        }

        if (consecutiveCount < VM_CONSECUTIVE_HITS) {
            return;  // Not enough consecutive detections yet
        }

        // Cooldown check
        unsigned long now = millis();
        if (now - lastCommandTime < VM_INFERENCE_COOLDOWN_MS) {
            return;  // Too soon since last command
        }

        // All checks passed — map to command
        ParsedCommand cmd;
        if (CommandMapper::mapLabelToCommand(topLabel, confidence, cmd)) {
            pendingCommand  = cmd;
            commandReady    = true;
            lastCommandTime = now;
            consecutiveCount = 0;   // Reset to avoid re-triggering

            Serial.print("VoiceRecognizer: >>> COMMAND: ");
            Serial.print(VM_LABELS[topLabel]);
            Serial.print(" (");
            Serial.print(confidence * 100, 1);
            Serial.println("%)");
        }
    }
};

// Required by Edge Impulse SDK
void ei_printf(const char *format, ...) {
    static char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
}

#endif // VOICE_RECOGNIZER_H
