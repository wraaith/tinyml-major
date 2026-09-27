/*
 * Audio Capture Module
 * PDM microphone driver with double-buffered recording
 * for continuous Edge Impulse inference.
 *
 * Hardware: Arduino Nano 33 BLE Sense on-board PDM mic
 * Based on Edge Impulse nano_ble33_sense_microphone_continuous example
 */

#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <Arduino.h>
#include <PDM.h>
#include "voice_model.h"

// ============================================================================
// DOUBLE-BUFFER INFERENCE STRUCTURE
// ============================================================================

typedef struct {
    int16_t *buffers[2];       // Two ping-pong buffers
    uint8_t  buf_select;       // Which buffer is currently being filled
    volatile uint8_t buf_ready; // Set to 1 when a buffer is full
    uint32_t buf_count;        // Current sample count in active buffer
    uint32_t n_samples;        // Target samples per buffer (slice size)
} inference_t;

// ============================================================================
// AUDIO CAPTURE CLASS
// ============================================================================

class AudioCapture {
private:
    static inference_t inference;
    static int16_t*    sampleBuffer;
    static bool        recordReady;

    // PDM callback — ISR context, keep it fast
    static void pdmDataReadyCallback() {
        int bytesAvailable = PDM.available();
        
        // Safety bounds check
        int maxBytes = inference.n_samples * sizeof(int16_t);
        if (bytesAvailable > maxBytes) {
            bytesAvailable = maxBytes;
        }

        int bytesRead = PDM.read((char *)sampleBuffer, bytesAvailable);

        if (!recordReady) return;

        int samplesRead = bytesRead / 2; // 16-bit samples
        for (int i = 0; i < samplesRead; i++) {
            inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

            if (inference.buf_count >= inference.n_samples) {
                inference.buf_select ^= 1;   // Swap buffer
                inference.buf_count = 0;
                inference.buf_ready = 1;
            }
        }
    }

public:
    // Start PDM capture for continuous inference
    bool begin(uint32_t sliceSize) {
        // Allocate double buffers
        inference.buffers[0] = (int16_t *)malloc(sliceSize * sizeof(int16_t));
        if (!inference.buffers[0]) {
            Serial.println("AudioCapture: Failed to allocate buffer 0");
            return false;
        }

        inference.buffers[1] = (int16_t *)malloc(sliceSize * sizeof(int16_t));
        if (!inference.buffers[1]) {
            free(inference.buffers[0]);
            Serial.println("AudioCapture: Failed to allocate buffer 1");
            return false;
        }

        // Temp buffer for PDM reads - allocated full slice size to prevent overflow
        sampleBuffer = (int16_t *)malloc(sliceSize * sizeof(int16_t));
        if (!sampleBuffer) {
            free(inference.buffers[0]);
            free(inference.buffers[1]);
            Serial.println("AudioCapture: Failed to allocate sample buffer");
            return false;
        }

        inference.buf_select = 0;
        inference.buf_count  = 0;
        inference.n_samples  = sliceSize;
        inference.buf_ready  = 0;

        // Configure PDM
        PDM.onReceive(pdmDataReadyCallback);
        PDM.setBufferSize(4096);  // Match EI example (2048 samples * 2 bytes)

        if (!PDM.begin(1, VM_SAMPLE_RATE_HZ)) {
            Serial.println("AudioCapture: PDM.begin() failed!");
            return false;
        }

        PDM.setGain(VM_PDM_GAIN);

        recordReady = true;

        Serial.println("AudioCapture: Initialized");
        Serial.print("  - Sample rate: ");
        Serial.print(VM_SAMPLE_RATE_HZ);
        Serial.println(" Hz");
        Serial.print("  - Slice size:  ");
        Serial.print(sliceSize);
        Serial.println(" samples");

        return true;
    }

    // Block until a full audio slice is ready.
    // Returns false if an overrun was detected.
    bool waitForSlice() {
        bool ret = true;

        if (inference.buf_ready == 1) {
            // Overrun — previous buffer was not consumed in time
            Serial.println("AudioCapture: WARNING buffer overrun");
            ret = false;
        }

        while (inference.buf_ready == 0) {
            delay(1);
        }

        inference.buf_ready = 0;
        return ret;
    }

    // Check if a slice is available (non-blocking)
    bool isSliceReady() const {
        return inference.buf_ready == 1;
    }

    // Consume the ready flag (call after reading the buffer)
    void consumeSlice() {
        inference.buf_ready = 0;
    }

    // Get pointer to the LAST COMPLETED buffer (the one NOT currently being filled)
    int16_t* getReadyBuffer() {
        return inference.buffers[inference.buf_select ^ 1];
    }

    // Edge Impulse signal callback — converts int16 → float
    // FIX: Edge Impulse expects floats in the raw int16 range (-32768 to 32767).
    // Normalizing to [-1.0, 1.0] by dividing by 32768 zeros out the signal power in the DSP block.
    static int getSignalData(size_t offset, size_t length, float *out_ptr) {
        // Read from the buffer that is NOT currently being written to
        int16_t *buf = inference.buffers[inference.buf_select ^ 1];
        for (size_t i = 0; i < length; i++) {
            out_ptr[i] = (float)buf[offset + i];
        }
        return 0;
    }

    // Shut down PDM and free memory
    void end() {
        recordReady = false;
        PDM.end();
        if (inference.buffers[0]) { free(inference.buffers[0]); inference.buffers[0] = nullptr; }
        if (inference.buffers[1]) { free(inference.buffers[1]); inference.buffers[1] = nullptr; }
        if (sampleBuffer) { free(sampleBuffer); sampleBuffer = nullptr; }
        Serial.println("AudioCapture: Stopped");
    }

    // Expose slice size for signal_t
    uint32_t getSliceSize() const { return inference.n_samples; }
};

// Static member definitions
inference_t AudioCapture::inference = {};
int16_t*    AudioCapture::sampleBuffer = nullptr;
bool        AudioCapture::recordReady  = false;

#endif // AUDIO_CAPTURE_H
