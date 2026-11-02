/*
 * Voice Model Configuration
 * Central header defining all model parameters for the TinyML voice model.
 *
 * Current Active Model:
 *   Project: trial33 (ID: 1153282)
 *   Source:  ei-trial33-arduino-1.0.1-impulse-#1
 *   Impulse: #1
 *   Target:  Arduino Nano 33 BLE Sense (Rev2 compatible)
 *   Quantization: Full INT8
 *   DSP: MFCC (13 cepstrals, 60 filters, 512-pt FFT, 2.0s window)
 *   Labels (6): "fan off", "fan on", "light off", "light on", "noise", "unknown"
 */

#ifndef VOICE_MODEL_H
#define VOICE_MODEL_H

// ============================================================================
// MODEL INTEGRATION TOGGLE
// ============================================================================
#define TINYML_MODEL_ENABLED       1

// ============================================================================
// MODEL IDENTITY
// ============================================================================

#define VM_PROJECT_ID              1153282
#define VM_PROJECT_NAME            "trial33"
#define VM_IMPULSE_ID              1
#define VM_DEPLOY_VERSION          1

// ============================================================================
// AUDIO / SAMPLING PARAMETERS
// ============================================================================

#define VM_SAMPLE_RATE_HZ          16000       // PDM sample rate (16 kHz standard)
#define VM_SAMPLE_LENGTH_MS        2000        // 32000 samples @ 16 kHz = 2.0 s window
#define VM_RAW_SAMPLE_COUNT        32000       // Total samples per inference window
#define VM_RAW_SAMPLES_PER_FRAME   1           // Mono
#define VM_INTERVAL_MS             0.0625f     // 1 / 16000

// Continuous-inference slicing
#define VM_SLICES_PER_WINDOW       4           // 4 slices -> 500 ms per slice
#define VM_SLICE_SIZE              (VM_RAW_SAMPLE_COUNT / VM_SLICES_PER_WINDOW)

// ============================================================================
// DSP (MFCC) PARAMETERS
// ============================================================================

#define VM_DSP_BLOCK_ID            6
#define VM_NN_INPUT_FRAME_SIZE     1287        // MFCC output features fed to NN
#define VM_NUM_CEPSTRAL            13
#define VM_FRAME_LENGTH            0.025f      // 25 ms
#define VM_FRAME_STRIDE            0.02f       // 20 ms
#define VM_NUM_FILTERS             60
#define VM_FFT_LENGTH              512
#define VM_NUM_FRAMES              99          // Total MFCC time frames: floor((32000-400)/320) + 1 = 99
#define VM_WIN_SIZE                151         // Internal EI DSP config window parameter (sample count)
#define VM_PRE_COF                 0.98f       // Pre-emphasis coefficient

// ============================================================================
// NEURAL NETWORK & QUANTIZATION
// ============================================================================

#define VM_QUANTIZED               1           // INT8 quantization enabled
#define VM_COMPILED                1           // EON Compiler used
#define VM_TFLITE_ARENA_SIZE       7801        // Bytes needed for TFLite arena
#define VM_INPUT_DATATYPE_INT8     1
#define VM_OUTPUT_DATATYPE_INT8    1

// INT8 dequantization parameters (from model_variables.h)
#define VM_OUTPUT_ZERO_POINT       (-128)
#define VM_OUTPUT_SCALE            0.00390625f // 1/256

// ============================================================================
// CLASSIFICATION LABELS (Matches ei_classifier_inferencing_categories)
// ============================================================================

#define VM_LABEL_COUNT             6
#define VM_NN_OUTPUT_COUNT         6

// Label indices (0-indexed matching model output order)
#define VM_LABEL_FAN_OFF           0
#define VM_LABEL_FAN_ON            1
#define VM_LABEL_LIGHT_OFF         2
#define VM_LABEL_LIGHT_ON          3
#define VM_LABEL_NOISE             4
#define VM_LABEL_UNKNOWN           5

static const char* VM_LABELS[VM_LABEL_COUNT] = {
    "fan off",
    "fan on",
    "light off",
    "light on",
    "noise",
    "unknown"
};

// ============================================================================
// INFERENCE THRESHOLDS & TUNING (Tuned for Strict Instructed Commands Only)
// ============================================================================

#define VM_CONFIDENCE_THRESHOLD    0.78f       // Strict gate: requires 78%+ certainty to actuate
#define VM_MIN_SEPARATION_MARGIN   0.20f       // Strict 20% margin over competing command (e.g. on vs off)
#define VM_MAX_NON_CMD_SUM         0.22f       // Noise + Unknown combined mass must be <= 22%
#define VM_MIN_SPEECH_PEAK         1500        // VAD energy gate: peak sample >= 1500 (ignores background silence)
#define VM_CONSECUTIVE_HITS        1           // Instant single-slice triggering
#define VM_INFERENCE_COOLDOWN_MS   2000        // Lockout matching full 2.0s window to prevent re-triggering (ms)
#define VM_PDM_GAIN                80          // PDM mic gain: 80 = 0x50 (+20 dB hardware gain). Calibrate to training.

#endif // VOICE_MODEL_H
