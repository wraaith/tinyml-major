# 🎙 TinyML Voice Studio (Standalone Desktop Application)

A dedicated, standalone desktop application and dataset curation suite for collecting voice commands in `.wav` format to train Keyword Spotting (KWS) and Speech Recognition models for microcontrollers (**Arduino Nano 33 BLE Sense Rev2**).

---

## ⚡ Quick Launch Options

### Option 1: Standalone Windows App (.exe) — No Terminal Needed!
Simply double-click:
`Launch_TinyML_Studio.bat`  
or directly open:  
[`app\TinyML-Voice-Studio\TinyML-Voice-Studio.exe`](file:///c:/Users/preet/code%20projts/tinyml-major/voice-data-collector/app/TinyML-Voice-Studio/TinyML-Voice-Studio.exe)

### Option 2: Run Python GUI Directly
```powershell
python gui_collector.py
```

### Option 3: Terminal CLI (Rapid-Fire Batch Mode)
```powershell
python collector.py
```

---

## 🖥 Desktop App Features

- **Spacebar Shortcut:** Press `Space` anywhere to start recording a sample.
- **Audio Cue Beeps:** 3.. 2.. 1.. audio countdown so you know exactly when to speak without staring at the screen.
- **⚡ Rapid-Fire Batch Mode:** Record 10, 20, or 50 samples in sequence with automatic intervals (~1-2 minutes for a full class).
- **Audio Playback & Delete:** Listen to the last recorded sample immediately, or hit `🗑 Delete Last` if you misspoke.
- **📂 Open Folder:** One-click button opens Windows File Explorer directly to your dataset.
- **📊 Export Splits:** Exports `labels.txt` and an 80/10/10 Train/Validation/Test `dataset_splits.json` for model training.
- **Microphone Selection:** Switch between connected microphones or headset devices.

---

## ⚙ Audio Specifications

Conforms strictly to TinyML / TensorFlow Lite Micro / Edge Impulse requirements:
- **Format:** `16-bit PCM WAV` (Mono)
- **Sample Rate:** `16,000 Hz` (16 kHz)
- **Duration:** `1.0s` (Configurable: 0.8s – 2.0s)

---

## 📂 Project Structure

```text
voice-data-collector/
├── Launch_TinyML_Studio.bat       # Double-click to launch standalone desktop app
├── app/
│   └── TinyML-Voice-Studio/
│       ├── TinyML-Voice-Studio.exe # Standalone Windows Executable
│       └── _internal/             # Packaged runtime & dependencies
├── dataset/                       # Organized audio dataset by class
│   ├── light_on/
│   ├── light_off/
│   ├── fan_on/
│   ├── fan_off/
│   ├── heater_on/
│   ├── heater_off/
│   ├── pump_on/
│   ├── pump_off/
│   ├── stop/
│   ├── background_noise/
│   └── silence/
├── gui_collector.py               # Desktop GUI application source
├── collector.py                   # Terminal CLI tool
├── dataset_stats.py               # Dataset validator & splitter
└── config.py                      # Audio specifications & settings
```
