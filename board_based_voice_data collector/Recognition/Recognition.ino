/* Audio Data Capture for Edge Impulse
 * Target: Arduino Nano 33 BLE Sense Rev2
 *
 * Captures audio clips from the onboard PDM microphone (MP34DT06JTR)
 * and streams them over Serial so a PC-side script can save them as .wav files
 * ready for upload to Edge Impulse Studio.
 *
 * The PC script sends configuration (frequency & duration) before recording.
 *
 * Wiring: None — uses the onboard digital microphone.
 */

#include <PDM.h>

// ----- Defaults (overridden by PC config command) --------------------------
static uint32_t sampleRate    = 16000;   // Hz
static uint32_t recordLenMs   = 1000;    // milliseconds
static uint32_t numSamples    = 16000;   // sampleRate * recordLenMs / 1000

#define NUM_CHANNELS 1  // Mono

// PDM callback buffer
static int16_t sampleBuffer[512];
static volatile int samplesRead = 0;

// Full clip buffer
static int16_t *recordBuffer = nullptr;
static uint32_t recordIndex  = 0;
static bool     recording    = false;
static uint32_t clipCount    = 0;
static bool     configured   = false;

// ----- PDM callback --------------------------------------------------------
void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

// ----- Allocate / reallocate the record buffer -----------------------------
bool allocateBuffer() {
  if (recordBuffer != nullptr) {
    free(recordBuffer);
    recordBuffer = nullptr;
  }
  numSamples = (uint32_t)((uint64_t)sampleRate * recordLenMs / 1000);
  recordBuffer = (int16_t *)malloc(numSamples * sizeof(int16_t));
  if (recordBuffer == nullptr) {
    Serial.println("ERROR: Could not allocate buffer!");
    return false;
  }
  return true;
}

// ----- Start the PDM mic with current settings -----------------------------
bool startMic() {
  PDM.end();  // Stop if already running
  delay(100);
  PDM.onReceive(onPDMdata);
  PDM.setBufferSize(1024);
  // PDM.setGain(20);  // Uncomment & adjust if audio is too quiet/loud
  if (!PDM.begin(NUM_CHANNELS, sampleRate)) {
    Serial.println("ERROR: Failed to start PDM!");
    return false;
  }
  return true;
}

// ----- Setup ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("READY");
}

// ----- Main loop -----------------------------------------------------------
void loop() {
  // --- Wait for commands from PC -------------------------------------------
  if (!recording && Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // CONFIG <sampleRate> <durationMs>
    if (cmd.startsWith("CONFIG")) {
      int firstSpace  = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (firstSpace > 0 && secondSpace > 0) {
        uint32_t newRate = cmd.substring(firstSpace + 1, secondSpace).toInt();
        uint32_t newLen  = cmd.substring(secondSpace + 1).toInt();

        // Validate
        if (newRate >= 8000 && newRate <= 41000 && newLen >= 100 && newLen <= 30000) {
          sampleRate  = newRate;
          recordLenMs = newLen;

          if (!allocateBuffer()) return;
          if (!startMic()) return;

          configured = true;
          Serial.print("CONFIG_OK ");
          Serial.print(sampleRate);
          Serial.print(" ");
          Serial.print(recordLenMs);
          Serial.print(" ");
          Serial.println(numSamples);
        } else {
          Serial.println("CONFIG_ERR Invalid values. Rate: 8000-41000, Length: 100-30000ms");
        }
      } else {
        Serial.println("CONFIG_ERR Usage: CONFIG <sampleRate> <durationMs>");
      }
    }

    // r — record a clip
    else if (cmd == "r" || cmd == "R") {
      if (!configured) {
        Serial.println("CONFIG_ERR Send CONFIG first");
        return;
      }
      recordIndex = 0;
      recording   = true;
      samplesRead = 0;
      digitalWrite(LED_BUILTIN, HIGH);  // LED on
      Serial.println("REC_START");
    }

    // q — quit
    else if (cmd == "q" || cmd == "Q") {
      Serial.println("QUIT");
      PDM.end();
      if (recordBuffer) free(recordBuffer);
      while (1);
    }
  }

  // --- Recording in progress ------------------------------------------------
  if (recording) {
    if (samplesRead > 0) {
      int count = samplesRead;
      samplesRead = 0;
      for (int i = 0; i < count && recordIndex < numSamples; i++) {
        recordBuffer[recordIndex++] = sampleBuffer[i];
      }
    }

    // --- Clip complete? -----------------------------------------------------
    if (recordIndex >= numSamples) {
      recording = false;
      digitalWrite(LED_BUILTIN, LOW);  // LED off
      clipCount++;

      Serial.print("AUDIO_START ");
      Serial.print(numSamples);
      Serial.print(" ");
      Serial.print(sampleRate);
      Serial.print(" 16");
      Serial.println();

      // Stream raw PCM bytes
      Serial.write((const uint8_t *)recordBuffer, numSamples * sizeof(int16_t));

      Serial.println();
      Serial.println("AUDIO_END");
    }
  }
}
