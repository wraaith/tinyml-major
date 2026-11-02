# TinyML Voice Model Integration (Active: trial33)

This directory manages the on-device voice recognition pipeline for the Smart Home Appliance Controller (Arduino Nano 33 BLE Sense).

---

## Architecture Overview

```
[ PDM Microphone (16 kHz mono) ]
             │
      audio_capture.h (Ring buffer / 500 ms slices)
             │
     voice_recognizer.h (Edge Impulse Continuous Classifier: trial33)
             │
      command_mapper.h (Maps winning label → ParsedCommand)
             │
        firmware.ino (Safety checks → ApplianceController / BLE)
```

---

## Active Model Specifications

| Parameter | Value |
|---|---|
| **Model Name** | `trial33` (Edge Impulse Project 1153282, Impulse #1) |
| **Inferencing Header** | `<trial33_inferencing.h>` |
| **Quantization** | Full INT8 |
| **Sample Rate** | 16,000 Hz (mono) |
| **Inference Window** | 2,000 ms (32,000 samples) |
| **Slices Per Window** | 4 (500 ms continuous slice) |
| **DSP** | MFCC (13 cepstrals, 60 filters, 512-pt FFT) |
| **TFLite Arena** | 7,801 bytes |

---

## Classification Labels & Command Mapping

| Index | Label | Mapped Action | Target |
|---|---|---|---|
| `0` | `"fan off"` | `CMD_OFF` | Appliance 2 (Fan, Pin 3) |
| `1` | `"fan on"` | `CMD_ON` (100%) | Appliance 2 (Fan, Pin 3) |
| `2` | `"light off"` | `CMD_OFF` | Appliance 1 (Light, Pin 2) |
| `3` | `"light on"` | `CMD_ON` (100%) | Appliance 1 (Light, Pin 2) |
| `4` | `"noise"` | *Ignored* | Background noise rejection |
| `5` | `"unknown"` | *Ignored* | Out-of-vocabulary speech rejection |

---

## Key Files

- [`voice_model.h`](voice_model.h): Active model defines, parameters, categories, and thresholds.
- [`voice_recognizer.h`](voice_recognizer.h): Continuous classification pipeline with debouncing and cooldown.
- [`command_mapper.h`](command_mapper.h): Mapping between model labels and appliance control commands.
- [`audio_capture.h`](audio_capture.h): Double-buffered PDM audio capture driver.
