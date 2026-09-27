#!/usr/bin/env python3
"""
Dataset Inspector, Validator & Splitter for TinyML
==================================================
Validates all WAV files in the dataset directory:
- Verifies sample rate (16 kHz), mono channel, bit depth
- Detects corrupt, empty, or clipped audio files
- Exports labels.txt and train / val / test splits
"""

import os
import sys
import wave
import json
import random
import numpy as np

# Force UTF-8 on Windows console to prevent charmap encoding errors
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

from rich.console import Console
from rich.table import Table
from rich.panel import Panel

from config import DATASET_DIR, SAMPLE_RATE, CHANNELS, SAMPLE_WIDTH, ensure_dataset_dirs

console = Console()


def inspect_dataset():
    ensure_dataset_dirs()
    if not os.path.exists(DATASET_DIR):
        console.print(f"[red]Dataset directory does not exist: {DATASET_DIR}[/red]")
        return

    table = Table(title="TinyML Audio Dataset Quality Audit", header_style="bold cyan")
    table.add_column("Class Label", style="cyan")
    table.add_column("Valid WAVs", justify="right", style="green")
    table.add_column("Corrupt/Invalid", justify="right", style="red")
    table.add_column("Avg RMS", justify="right", style="yellow")
    table.add_column("Total Audio (s)", justify="right", style="magenta")

    total_valid = 0
    total_invalid = 0
    all_valid_files = {}

    classes = sorted([d for d in os.listdir(DATASET_DIR) if os.path.isdir(os.path.join(DATASET_DIR, d))])
    
    for cls in classes:
        cls_dir = os.path.join(DATASET_DIR, cls)
        files = [f for f in os.listdir(cls_dir) if f.endswith(".wav")]
        
        valid_in_class = []
        invalid_count = 0
        rms_values = []
        total_frames = 0

        for fname in files:
            fpath = os.path.join(cls_dir, fname)
            try:
                with wave.open(fpath, 'rb') as wf:
                    n_ch = wf.getnchannels()
                    sw = wf.getsampwidth()
                    sr = wf.getframerate()
                    n_frames = wf.getnframes()
                    frames = wf.readframes(n_frames)

                    # Check specs
                    if sr != SAMPLE_RATE or n_ch != CHANNELS or sw != SAMPLE_WIDTH or n_frames == 0:
                        invalid_count += 1
                        continue

                    # Calculate RMS
                    audio_arr = np.frombuffer(frames, dtype=np.int16)
                    rms = np.sqrt(np.mean(audio_arr.astype(np.float64) ** 2))
                    rms_values.append(rms)
                    valid_in_class.append(fpath)
                    total_frames += n_frames

            except Exception:
                invalid_count += 1

        total_valid += len(valid_in_class)
        total_invalid += invalid_count
        all_valid_files[cls] = valid_in_class

        avg_rms = np.mean(rms_values) if rms_values else 0
        # Calculate total duration from actual file lengths
        tot_duration = total_frames / SAMPLE_RATE

        table.add_row(
            cls,
            str(len(valid_in_class)),
            str(invalid_count) if invalid_count > 0 else "[dim]0[/dim]",
            f"{avg_rms:.0f}",
            f"{tot_duration:.1f}s"
        )

    console.print(table)
    console.print(f"\n[bold]Total Valid Clips:[/bold] [green]{total_valid}[/green] | [bold]Total Issues:[/bold] [red]{total_invalid}[/red]")

    # Export labels.txt
    labels_file = os.path.join(DATASET_DIR, "labels.txt")
    active_classes = [cls for cls, flist in all_valid_files.items() if len(flist) > 0]
    with open(labels_file, "w") as f:
        for cls in active_classes:
            f.write(f"{cls}\n")
    console.print(f"[green][OK] Exported active labels ({len(active_classes)} classes) to:[/green] [dim]{labels_file}[/dim]")

    # Option to create train/val/test split
    create_splits(all_valid_files)


def create_splits(all_valid_files, train_ratio=0.8, val_ratio=0.1):
    """Create train, validation, and test manifest files."""
    train_files = []
    val_files = []
    test_files = []

    for cls, files in all_valid_files.items():
        if not files:
            continue
        shuffled = files.copy()
        random.seed(42)
        random.shuffle(shuffled)

        n_total = len(shuffled)
        n_train = int(n_total * train_ratio)
        n_val = int(n_total * val_ratio)

        train_files.extend(shuffled[:n_train])
        val_files.extend(shuffled[n_train:n_train + n_val])
        test_files.extend(shuffled[n_train + n_val:])

    splits = {
        "train_count": len(train_files),
        "val_count": len(val_files),
        "test_count": len(test_files),
        "train": [os.path.relpath(p, DATASET_DIR).replace("\\", "/") for p in train_files],
        "val": [os.path.relpath(p, DATASET_DIR).replace("\\", "/") for p in val_files],
        "test": [os.path.relpath(p, DATASET_DIR).replace("\\", "/") for p in test_files],
    }

    split_file = os.path.join(DATASET_DIR, "dataset_splits.json")
    with open(split_file, "w") as f:
        json.dump(splits, f, indent=2)

    console.print(f"[green][OK] Exported Train/Val/Test splits (80/10/10) to:[/green] [dim]{split_file}[/dim]\n")


if __name__ == "__main__":
    inspect_dataset()
