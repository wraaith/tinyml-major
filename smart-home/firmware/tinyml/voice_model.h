/*
 * Voice Model Configuration
 * Central header defining all model parameters for the int8 quantized
 * Edge Impulse model (Project 1122244, Impulse #3).
 *
 * Source: ei-tinyml-majar-2-arduino-1.0.1-impulse-#3
 * Quantization: Full INT8 (input & output)
 * DSP: MFCC (13 cepstrals, 32 filters, 256-pt FFT)
 * Labels: fan_off, fan_on, light_off, light_on, noise
 *
 * Hardware: Arduino Nano 33 BLE Sense (Rev2 compatible)
 */

#ifndef VOICE_MODEL_H
#define VOICE_MODEL_H

// ============================================================================
// MODEL IDENTITY
// ============================================================================

#define VM_PROJECT_ID              1122244
#define VM_PROJECT_NAME            "tinyml majar 2"
#define VM_IMPULSE_ID              3
#define VM_DEPLOY_VERSION          1

// ============================================================================
// AUDIO / SAMPLING PARAMETERS
// ============================================================================

#define VM_SAMPLE_RATE_HZ          16000       // PDM sample rate
#define VM_SAMPLE_LENGTH_MS        1000        // 16000 samples @ 16 kHz = 1 s
#define VM_RAW_SAMPLE_COUNT        16000       // Total samples per inference window
#define VM_RAW_SAMPLES_PER_FRAME   1           // Mono
#define VM_INTERVAL_MS             0.0625f     // 1 / 16000

// Continuous-inference slicing
#define VM_SLICES_PER_WINDOW       4           // 4 slices → 250 ms per slice
#define VM_SLICE_SIZE              (VM_RAW_SAMPLE_COUNT / VM_SLICES_PER_WINDOW)

// ============================================================================
// DSP (MFCC) PARAMETERS
// ============================================================================

#define VM_DSP_BLOCK_ID            10
#define VM_NN_INPUT_FRAME_SIZE     650         // MFCC output features fed to NN
#define VM_NUM_CEPSTRAL            13
#define VM_FRAME_LENGTH            0.02f       // 20 ms
#define VM_FRAME_STRIDE            0.02f       // 20 ms (no overlap)
#define VM_NUM_FILTERS             32
#define VM_FFT_LENGTH              256
#define VM_WIN_SIZE                101          // Number of MFCC frames
#define VM_PRE_COF                 0.98f       // Pre-emphasis coefficient

// ============================================================================
// NEURAL NETWORK & QUANTIZATION
// ============================================================================

#define VM_QUANTIZED               1           // INT8 quantization enabled
#define VM_COMPILED                1           // EON Compiler used
#define VM_TFLITE_ARENA_SIZE       6265        // Bytes needed for TFLite arena
#define VM_INPUT_DATATYPE_INT8     1
#define VM_OUTPUT_DATATYPE_INT8    1

// INT8 dequantization parameters (from model_variables.h)
#define VM_OUTPUT_ZERO_POINT       (-128)
#define VM_OUTPUT_SCALE            0.00390625f // 1/256

// ============================================================================
// CLASSIFICATION LABELS
// ============================================================================

#define VM_LABEL_COUNT             5
#define VM_NN_OUTPUT_COUNT         5

// Label indices (must match ei_classifier_inferencing_categories order)
#define VM_LABEL_FAN_OFF           0
#define VM_LABEL_FAN_ON            1
#define VM_LABEL_LIGHT_OFF         2
#define VM_LABEL_LIGHT_ON          3
#define VM_LABEL_NOISE             4

static const char* VM_LABELS[VM_LABEL_COUNT] = {
    "fan_off",
    "fan_on",
    "light_off",
    "light_on",
    "noise"
};

// ============================================================================
// INFERENCE THRESHOLDS
// ============================================================================

#define VM_CONFIDENCE_THRESHOLD    0.60f       // Minimum confidence to act
#define VM_NOISE_REJECT_THRESHOLD  0.40f       // Reject if noise > this
#define VM_CONSECUTIVE_HITS        1           // Require N consecutive same-class
                                               // detections before triggering action

// ============================================================================
// TIMING
// ============================================================================

#define VM_INFERENCE_COOLDOWN_MS   1500        // Min time between acted commands
#define VM_PDM_GAIN                80          // PDM microphone gain (Try 80 or 127 if training data was recorded loud)

#endif // VOICE_MODEL_H
