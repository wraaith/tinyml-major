/*
 * Feature Extraction Module
 * Wrapper around the Edge Impulse DSP pipeline for MFCC extraction.
 *
 * This module doesn't reimplement MFCC — it documents how the EI SDK
 * internally extracts features and provides helper utilities for
 * debugging and logging DSP parameters.
 *
 * The actual MFCC extraction happens inside run_classifier_continuous()
 * via the DSP block configured in model_variables.h.
 */

#ifndef FEATURE_EXTRACTION_H
#define FEATURE_EXTRACTION_H

#include <Arduino.h>
#include "voice_model.h"

// ============================================================================
// FEATURE EXTRACTION INFO (read-only documentation layer)
// ============================================================================

class FeatureExtraction {
public:
    // Print DSP configuration to Serial for debugging
    static void printConfig() {
        Serial.println("=== DSP / MFCC Configuration ===");
        Serial.print("  Block ID:       "); Serial.println(VM_DSP_BLOCK_ID);
        Serial.print("  Cepstral coeff: "); Serial.println(VM_NUM_CEPSTRAL);
        Serial.print("  Frame length:   "); Serial.print(VM_FRAME_LENGTH * 1000); Serial.println(" ms");
        Serial.print("  Frame stride:   "); Serial.print(VM_FRAME_STRIDE * 1000); Serial.println(" ms");
        Serial.print("  Num filters:    "); Serial.println(VM_NUM_FILTERS);
        Serial.print("  FFT length:     "); Serial.println(VM_FFT_LENGTH);
        Serial.print("  Win size:       "); Serial.println(VM_WIN_SIZE);
        Serial.print("  Pre-emphasis:   "); Serial.println(VM_PRE_COF);
        Serial.print("  NN input size:  "); Serial.println(VM_NN_INPUT_FRAME_SIZE);
        Serial.println("================================");
    }

    // Print quantization info
    static void printQuantizationInfo() {
        Serial.println("=== INT8 Quantization Info ===");
        Serial.print("  Input dtype:    INT8\n");
        Serial.print("  Output dtype:   INT8\n");
        Serial.print("  Zero point:     "); Serial.println(VM_OUTPUT_ZERO_POINT);
        Serial.print("  Scale:          "); Serial.println(VM_OUTPUT_SCALE, 8);
        Serial.print("  Arena size:     "); Serial.print(VM_TFLITE_ARENA_SIZE); Serial.println(" bytes");
        Serial.print("  EON compiled:   "); Serial.println(VM_COMPILED ? "Yes" : "No");
        Serial.println("==============================");
    }

    // Dequantize a single int8 output to float [0.0, 1.0]
    static float dequantize(int8_t quantized_val) {
        return ((float)quantized_val - (float)VM_OUTPUT_ZERO_POINT) * VM_OUTPUT_SCALE;
    }

    // Dequantize an array of int8 outputs to float
    static void dequantizeArray(const int8_t* input, float* output, size_t count) {
        for (size_t i = 0; i < count; i++) {
            output[i] = dequantize(input[i]);
        }
    }
};

#endif // FEATURE_EXTRACTION_H
