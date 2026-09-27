"""
Configuration & Audio Utilities for TinyML Voice Data Collector
Hardware target: Arduino Nano 33 BLE Sense (16kHz, 16-bit Mono PCM)
"""

import os
import sys
import wave
import numpy as np

# Audio Specifications (Standard for TinyML / TensorFlow Lite Micro / Edge Impulse)
SAMPLE_RATE = 16000     # 16 kHz
CHANNELS = 1            # Mono
DTYPE = np.int16        # 16-bit PCM
SAMPLE_WIDTH = 2        # 2 bytes per sample
DEFAULT_DURATION = 1.0  # 1.0 second per keyword clip

# Default Target Classes for Smart Home Controller
DEFAULT_CLASSES = [
    "light_on",
    "light_off",
    "fan_on",
    "fan_off",
    "heater_on",
    "heater_off",
    "pump_on",
    "pump_off",
    "stop",
    "background_noise",
    "silence"
]

# Base Dataset Directory
# When frozen by PyInstaller, locate dataset in the directory containing the .exe
if getattr(sys, 'frozen', False):
    BASE_DIR = os.path.dirname(sys.executable)
else:
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))

DATASET_DIR = os.path.join(BASE_DIR, "dataset")

def ensure_dataset_dirs():
    """Ensure the dataset directory and default class subdirectories exist."""
    os.makedirs(DATASET_DIR, exist_ok=True)
    for cls in DEFAULT_CLASSES:
        os.makedirs(os.path.join(DATASET_DIR, cls), exist_ok=True)

def save_wav(filepath: str, audio_data: np.ndarray, sample_rate: int = SAMPLE_RATE):
    """
    Save 1D numpy array (int16) as a standard PCM WAV file.
    """
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    
    # Flatten if multi-dimensional
    if audio_data.ndim > 1:
        audio_data = audio_data.squeeze()
        
    # Ensure int16
    if audio_data.dtype != np.int16:
        if audio_data.dtype == np.float32 or audio_data.dtype == np.float64:
            # Scale float (-1.0 to 1.0) to int16
            audio_data = np.clip(audio_data, -1.0, 1.0)
            audio_data = (audio_data * 32767.0).astype(np.int16)
        else:
            audio_data = audio_data.astype(np.int16)

    with wave.open(filepath, 'wb') as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(sample_rate)
        wf.writeframes(audio_data.tobytes())

def analyze_audio(audio_data: np.ndarray):
    """
    Analyze recording levels to verify if audio was captured properly.
    Returns:
        rms: Root Mean Square energy (0 to 32767)
        peak: Maximum absolute amplitude
        is_silent: True if RMS is abnormally low
        is_clipped: True if peak hits maximum digital ceiling
    """
    if audio_data.size == 0:
        return 0, 0, True, False
        
    data_float = audio_data.astype(np.float64)
    rms = np.sqrt(np.mean(data_float ** 2))
    peak = np.max(np.abs(data_float))
    
    # Thresholds:
    # int16 ranges from -32768 to 32767
    is_silent = rms < 50.0        # Microphone was muted or too quiet
    is_clipped = peak >= 32760.0  # Digital clipping distortion
    
    return float(rms), float(peak), is_silent, is_clipped
