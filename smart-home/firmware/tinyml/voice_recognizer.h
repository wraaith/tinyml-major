/*
 * Voice Recognizer Module
 * Orchestrates continuous audio capture -> Edge Impulse inference -> command output.
 *
 * Current Active Model: trial33 (Edge Impulse Project 1153282)
 *
 * Hardware: Arduino Nano 33 BLE Sense (PDM microphone)
 */

#ifndef VOICE_RECOGNIZER_H
#define VOICE_RECOGNIZER_H

#include <Arduino.h>
#include "voice_model.h"

#if TINYML_MODEL_ENABLED
    // Edge Impulse SDK settings — must be defined before inferencing header
    #define EIDSP_QUANTIZE_FILTERBANK              0
    #define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW  VM_SLICES_PER_WINDOW

    #include <trial33_inferencing.h>
#endif

#include "audio_capture.h"
#include "feature_extraction.h"
#include "command_mapper.h"

#if TINYML_MODEL_ENABLED

// ============================================================================
// ACTIVE TINYML VOICE RECOGNIZER
// ============================================================================

class VoiceRecognizer {
private:
    AudioCapture    audioCapture;
    bool            initialized;
    bool            enabled;

    // Adjustable confidence threshold (defaults to VM_CONFIDENCE_THRESHOLD)
    float           confidenceThreshold;

    // Cooldown lockout to avoid rolling-window re-triggering
    unsigned long   lastCommandTime;
    unsigned long   lastHeardTime;

    // Result accumulation — track initial fill period
    int             sliceCounter;

    // Rolling VAD peak tracking across all window slices (4 slices = 2.0s)
    int16_t         recentPeaks[VM_SLICES_PER_WINDOW];
    int             peakIndex;

    // Latest ready command
    bool            commandReady;
    ParsedCommand   pendingCommand;

    // Debug flag
    bool            debugMode;

public:
    VoiceRecognizer()
        : initialized(false)
        , enabled(true)
        , confidenceThreshold(VM_CONFIDENCE_THRESHOLD)
        , lastCommandTime(0)
        , lastHeardTime(0)
        , sliceCounter(-(VM_SLICES_PER_WINDOW))
        , peakIndex(0)
        , commandReady(false)
        , debugMode(false)
    {
        for (int i = 0; i < VM_SLICES_PER_WINDOW; i++) {
            recentPeaks[i] = 0;
        }
    }

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    bool begin() {
        run_classifier_init();

        if (!audioCapture.begin(EI_CLASSIFIER_SLICE_SIZE)) {
            return false;
        }

        AudioCapture::clearOverrun();
        confidenceThreshold = VM_CONFIDENCE_THRESHOLD;
        sliceCounter        = -(VM_SLICES_PER_WINDOW);
        peakIndex           = 0;
        for (int i = 0; i < VM_SLICES_PER_WINDOW; i++) {
            recentPeaks[i] = 0;
        }
        lastCommandTime     = 0;
        lastHeardTime       = 0;
        commandReady        = false;
        initialized         = true;
        return true;
    }

    void end() {
        if (initialized) {
            audioCapture.end();
            initialized = false;
        }
    }

    // ========================================================================
    // MAIN UPDATE — call this every loop() iteration
    // ========================================================================

    void update() {
        if (!initialized || !enabled) return;

        // Check for audio buffer overrun
        if (AudioCapture::hasOverrun()) {
            Serial.print("[TinyML Audio] WARNING: Buffer overrun detected (count: ");
            Serial.print(AudioCapture::getOverrunCount());
            Serial.println("). Main loop stalled > 500 ms!");
            AudioCapture::clearOverrun();
        }

        if (!audioCapture.isSliceReady()) return;

        // Peak audio energy gate: measure signal volume in the slice and update rolling peak window
        int16_t slicePeak = audioCapture.getSlicePeak();
        recentPeaks[peakIndex] = slicePeak;
        peakIndex = (peakIndex + 1) % VM_SLICES_PER_WINDOW;

        int16_t vadPeak = 0;
        for (int i = 0; i < VM_SLICES_PER_WINDOW; i++) {
            if (recentPeaks[i] > vadPeak) vadPeak = recentPeaks[i];
        }

        audioCapture.consumeSlice();

        signal_t signal;
        signal.total_length = audioCapture.getSliceSize();
        signal.get_data     = &AudioCapture::getSignalData;

        ei_impulse_result_t result = {0};
        EI_IMPULSE_ERROR err = run_classifier_continuous(&signal, &result, false);

        if (err != EI_IMPULSE_OK) {
            return;
        }

        sliceCounter++;

        // Wait until circular rolling buffer has filled initial slices
        if (sliceCounter >= 0) {
            processResult(result, vadPeak, slicePeak);
        }
    }

    // ========================================================================
    // COMMAND ACCESS & SENSITIVITY TUNING
    // ========================================================================

    bool hasCommand() const {
        return commandReady;
    }

    ParsedCommand getCommand() {
        commandReady = false;
        return pendingCommand;
    }

    void setEnabled(bool en) {
        enabled = en;
    }

    bool isEnabled()     const { return enabled; }
    bool isInitialized() const { return initialized; }

    void setDebug(bool dbg) { debugMode = dbg; }
    bool isDebug() const { return debugMode; }

    void adjustThreshold(float delta) {
        confidenceThreshold += delta;
        if (confidenceThreshold < 0.50f) confidenceThreshold = 0.50f;
        if (confidenceThreshold > 0.95f) confidenceThreshold = 0.95f;
        Serial.print("[TinyML] Adjusted Confidence Threshold: ");
        Serial.print((int)(confidenceThreshold * 100));
        Serial.println("%");
    }

    float getThreshold() const { return confidenceThreshold; }

    void printThresholds() const {
        Serial.println("\n--- TinyML Voice Gating Parameters ---");
        Serial.print("  Target Commands:      \"fan on\", \"fan off\", \"light on\", \"light off\"\n");
        Serial.print("  Confidence Threshold: "); Serial.print((int)(confidenceThreshold * 100)); Serial.println("%");
        Serial.print("  Separation Margin:    "); Serial.print((int)(VM_MIN_SEPARATION_MARGIN * 100)); Serial.println("% (prevents on vs off ambiguity)");
        Serial.print("  Max Non-Command Sum:  "); Serial.print((int)(VM_MAX_NON_CMD_SUM * 100)); Serial.println("% (noise + unknown must be <= 22%)");
        Serial.print("  VAD Energy Floor:     Peak >= "); Serial.print(VM_MIN_SPEECH_PEAK); Serial.println(" (rejects quiet ambient noise)");
        Serial.print("  Lockout Cooldown:     "); Serial.print(VM_INFERENCE_COOLDOWN_MS); Serial.println(" ms (full window clearing)");
        Serial.println("--------------------------------------\n");
    }

private:
    // ========================================================================
    // RESULT PROCESSING & RECOGNITION DECISION (STRICT INSTRUCTED COMMANDS)
    // ========================================================================

    void processResult(const ei_impulse_result_t& result, int16_t vadPeak, int16_t slicePeak) {
        // Raw scores from current slice
        float rawScores[VM_LABEL_COUNT];
        for (int i = 0; i < VM_LABEL_COUNT; i++) {
            rawScores[i] = result.classification[i].value;
        }

        // Live debug stream (when toggled on with 'd' in Serial Monitor)
        if (debugMode) {
            Serial.print("[Raw] ");
            for (int i = 0; i < VM_LABEL_COUNT; i++) {
                Serial.print(VM_LABELS[i]);
                Serial.print(":");
                Serial.print((int)(rawScores[i] * 100));
                Serial.print("% ");
            }
            Serial.print("| VAD Peak: ");
            Serial.print(vadPeak);
            Serial.print(" (slice: ");
            Serial.print(slicePeak);
            Serial.print(") | DSP: ");
            Serial.print(result.timing.dsp);
            Serial.print("ms NN: ");
            Serial.print(result.timing.classification);
            Serial.println("ms");
        }

        // Find the top candidate among the 4 actionable command classes (indices 0..3)
        int   bestCmd       = -1;
        float bestCmdScore  = 0.0f;
        float runnerUpScore = 0.0f;

        for (int i = 0; i < 4; i++) {
            if (rawScores[i] > bestCmdScore) {
                runnerUpScore = bestCmdScore;
                bestCmdScore  = rawScores[i];
                bestCmd       = i;
            } else if (rawScores[i] > runnerUpScore) {
                runnerUpScore = rawScores[i];
            }
        }

        float noiseScore   = rawScores[VM_LABEL_NOISE];
        float unknownScore = rawScores[VM_LABEL_UNKNOWN];
        float nonCmdSum    = noiseScore + unknownScore;
        unsigned long now  = millis();

        // If in cooldown period, ignore trailing activations from the 2.0s rolling window
        if (now - lastCommandTime < VM_INFERENCE_COOLDOWN_MS) {
            return;
        }

        // Strict criteria for ONLY INSTRUCTED COMMANDS:
        // 1. Acoustic Energy Gate: rolling window peak >= 1500 (ignores background room murmur / silence)
        // 2. Strict Confidence: confidence >= confidenceThreshold (default 78%, dynamically adjustable)
        // 3. Noise Dominance: command score must beat noise by at least 10% AND exceed double the noise score
        // 4. Unknown Dominance: command score must beat unknown non-target words decisively
        // 5. Clean Signal: noise + unknown combined mass must be <= 22%
        // 6. Decisive Separation: winner must outscore runner-up command by at least 20% margin
        bool energyOk      = (vadPeak >= VM_MIN_SPEECH_PEAK);
        bool confidenceOk  = (bestCmdScore >= confidenceThreshold);
        bool beatsNoise    = (bestCmdScore > (noiseScore + 0.10f)) && (bestCmdScore > (noiseScore * 2.0f));
        bool beatsUnknown  = (bestCmdScore > unknownScore) && (bestCmdScore > (unknownScore * 1.5f));
        bool cleanSignal   = (nonCmdSum <= VM_MAX_NON_CMD_SUM);
        bool beatsRunnerUp = (bestCmdScore >= (runnerUpScore + VM_MIN_SEPARATION_MARGIN));

        if (energyOk && confidenceOk && beatsNoise && beatsUnknown && cleanSignal && beatsRunnerUp) {
            // Verified instructed command! Map and actuate using the dynamic confidenceThreshold
            ParsedCommand cmd;
            if (CommandMapper::mapLabelToCommand(bestCmd, bestCmdScore, cmd, confidenceThreshold)) {
                pendingCommand  = cmd;
                commandReady    = true;
                lastCommandTime = now;

                // Clear rolling peaks on command trigger to prevent lingering audio energy
                for (int i = 0; i < VM_SLICES_PER_WINDOW; i++) {
                    recentPeaks[i] = 0;
                }

                Serial.println();
                Serial.print(">>> [INSTRUCTED COMMAND RECOGNIZED] \"");
                Serial.print(VM_LABELS[bestCmd]);
                Serial.print("\" (Confidence: ");
                Serial.print((int)(bestCmdScore * 100));
                Serial.print("%, Peak: ");
                Serial.print(vadPeak);
                Serial.print(" | DSP: ");
                Serial.print(result.timing.dsp);
                Serial.print("ms, NN: ");
                Serial.print(result.timing.classification);
                Serial.println("ms)");
            }
            return;
        }

        // Real-time hearing feedback & gate diagnostics:
        // If a command candidate is heard with reasonable volume (>= 40%) but held back by strict gates,
        // log the specific failing gate(s) to facilitate room acoustic tuning.
        if (bestCmdScore >= 0.40f && (energyOk || debugMode) && (now - lastHeardTime > 600)) {
            lastHeardTime = now;
            Serial.print(">>> [Candidate] \"");
            Serial.print(VM_LABELS[bestCmd]);
            Serial.print("\" ");
            Serial.print((int)(bestCmdScore * 100));
            Serial.print("% | Gating check: ");

            if (!energyOk) {
                Serial.print("[Energy "); Serial.print(vadPeak); Serial.print(" < 1500] ");
            }
            if (!confidenceOk) {
                Serial.print("[Conf "); Serial.print((int)(bestCmdScore * 100));
                Serial.print("% < "); Serial.print((int)(confidenceThreshold * 100)); Serial.print("%] ");
            }
            if (!beatsNoise) {
                Serial.print("[Noise "); Serial.print((int)(noiseScore * 100)); Serial.print("%] ");
            }
            if (!beatsUnknown) {
                Serial.print("[Unknown "); Serial.print((int)(unknownScore * 100)); Serial.print("%] ");
            }
            if (!cleanSignal) {
                Serial.print("[NonCmdSum "); Serial.print((int)(nonCmdSum * 100)); Serial.print("% > 22%] ");
            }
            if (!beatsRunnerUp) {
                Serial.print("[Margin "); Serial.print((int)((bestCmdScore - runnerUpScore) * 100)); Serial.print("% < 20%] ");
            }
            Serial.println();
        }
    }
};

#else // !TINYML_MODEL_ENABLED

class VoiceRecognizer {
private:
    bool enabled;
public:
    VoiceRecognizer() : enabled(false) {}
    bool begin() { return false; }
    void end() {}
    void update() {}
    bool hasCommand() const { return false; }
    ParsedCommand getCommand() { return ParsedCommand(); }
    void setEnabled(bool en) { enabled = en; }
    bool isEnabled() const { return enabled; }
    bool isInitialized() const { return false; }
    void setDebug(bool dbg) { (void)dbg; }
    bool isDebug() const { return false; }
    void adjustThreshold(float delta) { (void)delta; }
    float getThreshold() const { return 0.0f; }
    void printThresholds() const {}
};

#endif // TINYML_MODEL_ENABLED

inline void ei_printf(const char *format, ...) {
    static char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.print(buf);
}

#endif // VOICE_RECOGNIZER_H
