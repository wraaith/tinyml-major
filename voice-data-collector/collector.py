#!/usr/bin/env python3
"""
TinyML Voice Dataset Collector (CLI)
=====================================
A high-efficiency audio recording tool for training Keyword Spotting (KWS) 
and speech recognition models on microcontrollers (Arduino Nano 33 BLE Sense).

Specifications:
- 16,000 Hz Sampling Rate
- 16-bit PCM Mono WAV format
- Fixed-duration keyword segments (Default: 1.0s)
"""

import os
import sys
import time
import datetime
import winsound
import numpy as np

# Force UTF-8 on Windows console
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

import sounddevice as sd
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.prompt import Prompt, Confirm, IntPrompt
from rich.progress import Progress, BarColumn, TextColumn, TimeRemainingColumn

from config import (
    SAMPLE_RATE,
    CHANNELS,
    DTYPE,
    DEFAULT_DURATION,
    DEFAULT_CLASSES,
    DATASET_DIR,
    save_wav,
    analyze_audio,
    ensure_dataset_dirs
)

console = Console()

# Global state
CURRENT_DEVICE = None
SPEAKER_ID = "speaker1"
SAMPLE_DURATION = DEFAULT_DURATION
AUDIO_BEEPS = True


def play_beep(freq: int, duration_ms: int):
    """Play audio cue if enabled (Windows)."""
    if AUDIO_BEEPS:
        try:
            winsound.Beep(freq, duration_ms)
        except Exception:
            pass


def list_audio_devices():
    """List and allow selecting input audio devices."""
    global CURRENT_DEVICE
    devices = sd.query_devices()
    input_devices = []
    
    table = Table(title="Available Audio Input Devices", header_style="bold cyan")
    table.add_column("Index", justify="right", style="cyan")
    table.add_column("Device Name", style="white")
    table.add_column("Max Input Ch.", justify="center", style="green")
    table.add_column("Default", justify="center", style="yellow")

    default_in = sd.default.device[0]
    for idx, dev in enumerate(devices):
        if dev['max_input_channels'] > 0:
            input_devices.append(idx)
            is_def = "[YES]" if idx == default_in or idx == CURRENT_DEVICE else ""
            table.add_row(str(idx), dev['name'], str(dev['max_input_channels']), is_def)

    console.print(table)
    choice = Prompt.ask("Select device index (or press Enter to keep current)", default="")
    if choice.isdigit() and int(choice) in input_devices:
        CURRENT_DEVICE = int(choice)
        console.print(f"[bold green]Selected device {CURRENT_DEVICE}: {devices[CURRENT_DEVICE]['name']}[/bold green]\n")
    else:
        console.print("[dim]Using default system device.[/dim]\n")


def record_clip(duration: float = SAMPLE_DURATION) -> np.ndarray:
    """Record a single clip with specified duration."""
    samples = int(duration * SAMPLE_RATE)
    recording = sd.rec(
        samples,
        samplerate=SAMPLE_RATE,
        channels=CHANNELS,
        dtype=DTYPE,
        device=CURRENT_DEVICE
    )
    sd.wait()
    return recording.squeeze()


def test_microphone():
    """Live ASCII VUMeter to verify microphone levels and avoid muted recordings."""
    console.print("\n[bold cyan]=== Live Microphone Sound Check ===[/bold cyan]")
    console.print("[dim]Speak into your microphone to verify volume levels. Press Ctrl+C to stop.[/dim]\n")
    
    block_size = int(SAMPLE_RATE * 0.1)  # 100ms blocks
    try:
        with sd.InputStream(samplerate=SAMPLE_RATE, channels=CHANNELS, dtype=DTYPE,
                            device=CURRENT_DEVICE, blocksize=block_size) as stream:
            while True:
                data, _ = stream.read(block_size)
                rms, peak, is_silent, is_clipped = analyze_audio(data)
                
                # Normalize peak to 0-40 bar
                bar_len = min(40, int((peak / 32768.0) * 40))
                meter = "#" * bar_len + "-" * (40 - bar_len)
                
                status_txt = "CLIPPING!" if is_clipped else ("GOOD" if not is_silent else "LOW / SILENT")
                
                sys.stdout.write(f"\rLevel: [{meter}] Peak: {int(peak):5d} | Status: {status_txt}    ")
                sys.stdout.flush()
                time.sleep(0.05)
    except KeyboardInterrupt:
        console.print("\n[bold green]Microphone test finished.[/bold green]\n")


def select_label() -> str:
    """Prompt user to select a target class from list or type custom label."""
    console.print("\n[bold]Select Target Word / Command Class:[/bold]")
    for idx, cls in enumerate(DEFAULT_CLASSES, 1):
        # Count existing samples
        cls_dir = os.path.join(DATASET_DIR, cls)
        count = len([f for f in os.listdir(cls_dir) if f.endswith(".wav")]) if os.path.exists(cls_dir) else 0
        console.print(f"  [cyan]{idx:2d}[/cyan]. [white]{cls:<18}[/white] [dim]({count} samples)[/dim]")
    console.print(f"  [cyan]{len(DEFAULT_CLASSES) + 1:2d}[/cyan]. [italic yellow]+ Type Custom Class Label[/italic yellow]")

    while True:
        choice = Prompt.ask("Choose option", default="1")
        if choice.isdigit():
            c = int(choice)
            if 1 <= c <= len(DEFAULT_CLASSES):
                return DEFAULT_CLASSES[c - 1]
            elif c == len(DEFAULT_CLASSES) + 1:
                custom = Prompt.ask("Enter custom class name (e.g. 'bedroom_light')").strip().lower()
                custom = "".join(c if c.isalnum() or c == '_' else '_' for c in custom)
                if custom:
                    os.makedirs(os.path.join(DATASET_DIR, custom), exist_ok=True)
                    return custom
        console.print("[red]Invalid selection. Try again.[/red]")


def single_record_mode():
    """Record one sample with instant review/playback and save confirmation."""
    label = select_label()
    target_dir = os.path.join(DATASET_DIR, label)
    os.makedirs(target_dir, exist_ok=True)

    console.print(f"\n[bold green]Target Label:[/bold green] [bold cyan]{label}[/bold cyan] ({SAMPLE_DURATION}s)")
    Prompt.ask("Press [bold yellow]Enter[/bold yellow] when ready to record")

    # Countdown
    for i in range(3, 0, -1):
        console.print(f"[bold yellow]{i}...[/bold yellow]", end=" ", flush=True)
        play_beep(600, 100)
        time.sleep(0.6)
    
    console.print("[bold red][REC] SPEAK NOW![/bold red]")
    play_beep(1000, 150)
    audio = record_clip(SAMPLE_DURATION)
    play_beep(400, 100)
    console.print("[bold green][OK] Recording Finished![/bold green]\n")

    rms, peak, is_silent, is_clipped = analyze_audio(audio)
    console.print(f"[dim]Audio Stats -- RMS: {rms:.1f} | Peak: {peak:.0f}[/dim]")
    if is_silent:
        console.print("[bold red][!] Warning: Clip seems very quiet or silent! Check microphone.[/bold red]")
    if is_clipped:
        console.print("[bold yellow][!] Warning: Audio clipped (peak maxed out). Lower mic gain or speak further away.[/bold yellow]")

    # Review options
    while True:
        action = Prompt.ask(
            "Action",
            choices=["s", "p", "r", "d"],
            default="s"
        )
        if action == "s":
            # Save
            ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{SPEAKER_ID}_{label}_{ts}.wav"
            filepath = os.path.join(target_dir, filename)
            save_wav(filepath, audio, SAMPLE_RATE)
            console.print(f"[bold green][OK] Saved to:[/bold green] {filepath}")
            break
        elif action == "p":
            console.print("[cyan]Playing back recording...[/cyan]")
            sd.play(audio, SAMPLE_RATE)
            sd.wait()
        elif action == "r":
            console.print("[yellow]Re-recording...[/yellow]")
            return single_record_mode()
        elif action == "d":
            console.print("[dim]Discarded sample.[/dim]")
            break


def rapid_fire_batch_mode():
    """
    Rapid-fire collection mode:
    Records N samples in a row with countdown beeps and a 1.5s breather in between.
    Allows recording 20-50 high-quality samples in just ~1-2 minutes!
    """
    label = select_label()
    target_dir = os.path.join(DATASET_DIR, label)
    os.makedirs(target_dir, exist_ok=True)

    count = IntPrompt.ask(f"How many samples of '[bold cyan]{label}[/bold cyan]' would you like to collect?", default=10)
    delay_between = 1.2  # seconds breather

    console.print(Panel(
        f"[bold]Rapid-Fire Recording Plan[/bold]\n\n"
        f"- Label: [bold cyan]{label}[/bold cyan]\n"
        f"- Count: [bold yellow]{count}[/bold yellow] samples\n"
        f"- Duration: [bold]{SAMPLE_DURATION}s[/bold] each\n"
        f"- Cue: Beep -> Say [bold red]'{label}'[/bold red] clearly -> Pause -> Repeat\n\n"
        f"[dim]Tip: Vary your intonation, volume, and distance slightly for better ML generalization![/dim]",
        title="Batch Mode",
        border_style="cyan"
    ))

    Prompt.ask("Press [bold yellow]Enter[/bold yellow] to start batch sequence")

    saved_count = 0
    discarded_count = 0

    for i in range(1, count + 1):
        console.print(f"\n[bold cyan]--- Sample {i} of {count} ---[/bold cyan]")
        time.sleep(0.4)
        
        # Short countdown
        play_beep(600, 80)
        time.sleep(0.4)
        play_beep(600, 80)
        time.sleep(0.4)
        play_beep(1200, 150)
        console.print(f"[bold red][REC] Say: '{label.upper()}' NOW![/bold red]")

        audio = record_clip(SAMPLE_DURATION)
        play_beep(400, 100)

        rms, peak, is_silent, is_clipped = analyze_audio(audio)
        if is_silent:
            console.print(f"  [red][X] Silent clip detected (RMS: {rms:.0f}). Discarded.[/red]")
            discarded_count += 1
        else:
            ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{SPEAKER_ID}_{label}_{ts}_{i:03d}.wav"
            filepath = os.path.join(target_dir, filename)
            save_wav(filepath, audio, SAMPLE_RATE)
            clip_warn = " [yellow](clipped)[/yellow]" if is_clipped else ""
            console.print(f"  [green][OK] Saved sample {i}:[/green] [dim]{filename}[/dim] (RMS: {rms:.0f}){clip_warn}")
            saved_count += 1

        if i < count:
            time.sleep(delay_between)

    console.print(Panel(
        f"[bold green]Batch Complete![/bold green]\n"
        f"- Successfully saved: [bold cyan]{saved_count}[/bold cyan] samples\n"
        f"- Discarded (silent): [bold yellow]{discarded_count}[/bold yellow] samples\n"
        f"- Folder: [dim]{target_dir}[/dim]",
        title="Finished",
        border_style="green"
    ))


def background_noise_mode():
    """
    Records continuous background room noise and slices it into 1.0s chunks.
    Crucial for training TinyML models to suppress false activations.
    """
    target_dir = os.path.join(DATASET_DIR, "background_noise")
    os.makedirs(target_dir, exist_ok=True)

    console.print(Panel(
        "[bold]Background Noise Collection[/bold]\n\n"
        "TinyML models require background noise samples (fan hum, typing, silence, room tone)\n"
        "so they don't trigger false positives when no one is speaking.\n\n"
        "This tool records continuous ambient audio and splits it into standard 1.0s WAV chunks.",
        border_style="magenta"
    ))

    total_seconds = IntPrompt.ask("How many seconds of background noise to record?", default=30)
    console.print(f"[yellow]Will create approximately {int(total_seconds / SAMPLE_DURATION)} training clips.[/yellow]")
    Prompt.ask("Press [bold yellow]Enter[/bold yellow] to begin recording background noise (be natural, let ambient sounds happen)")

    console.print(f"[bold red][REC] Recording {total_seconds} seconds of ambient noise...[/bold red]")
    samples = int(total_seconds * SAMPLE_RATE)
    recording = sd.rec(
        samples,
        samplerate=SAMPLE_RATE,
        channels=CHANNELS,
        dtype=DTYPE,
        device=CURRENT_DEVICE
    )
    sd.wait()
    console.print("[bold green][OK] Recording complete! Slicing into 1.0s chunks...[/bold green]")

    flat_audio = recording.squeeze()
    chunk_samples = int(SAMPLE_DURATION * SAMPLE_RATE)
    num_chunks = len(flat_audio) // chunk_samples

    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    for i in range(num_chunks):
        chunk = flat_audio[i * chunk_samples : (i + 1) * chunk_samples]
        filename = f"bg_{ts}_{i:03d}.wav"
        save_wav(os.path.join(target_dir, filename), chunk, SAMPLE_RATE)

    console.print(f"[bold green][OK] Successfully created {num_chunks} background noise WAV files in 'background_noise/'[/bold green]\n")


def show_dataset_stats():
    """Display comprehensive dataset breakdown."""
    ensure_dataset_dirs()
    table = Table(title="TinyML Voice Dataset Summary", header_style="bold magenta")
    table.add_column("Class Label", style="cyan")
    table.add_column("Sample Count", justify="right", style="green")
    table.add_column("Total Duration", justify="right", style="yellow")
    table.add_column("Status / Recommendation", style="white")

    total_samples = 0
    all_classes = sorted(os.listdir(DATASET_DIR))

    for cls in all_classes:
        cls_path = os.path.join(DATASET_DIR, cls)
        if os.path.isdir(cls_path):
            wav_files = [f for f in os.listdir(cls_path) if f.endswith(".wav")]
            count = len(wav_files)
            total_samples += count
            duration_sec = count * SAMPLE_DURATION
            
            # Advice
            if count == 0:
                status = "[dim red]No samples yet[/dim red]"
            elif count < 15:
                status = "[yellow]Needs more (aim for 25-50)[/yellow]"
            elif count < 50:
                status = "[green]Good initial set[/green]"
            else:
                status = "[bold green]Excellent volume![/bold green]"

            table.add_row(cls, str(count), f"{duration_sec:.1f}s", status)

    console.print(table)
    console.print(f"[bold]Total Dataset Samples:[/bold] [bold cyan]{total_samples}[/bold cyan] ([yellow]{(total_samples * SAMPLE_DURATION):.1f}s[/yellow] total audio)")
    console.print(f"[dim]Dataset Location: {DATASET_DIR}[/dim]\n")


def settings_menu():
    """Configure speaker ID, duration, and audio beeps."""
    global SPEAKER_ID, SAMPLE_DURATION, AUDIO_BEEPS
    console.print("\n[bold cyan]=== Collection Settings ===[/bold cyan]")
    SPEAKER_ID = Prompt.ask("Speaker ID / Tag (e.g. 'speaker1', 'userA')", default=SPEAKER_ID)
    
    dur_str = Prompt.ask("Clip Duration in seconds (Standard TinyML is 1.0s)", default=str(SAMPLE_DURATION))
    try:
        SAMPLE_DURATION = float(dur_str)
    except ValueError:
        console.print("[red]Invalid duration. Keeping previous value.[/red]")
        
    AUDIO_BEEPS = Confirm.ask("Enable audio countdown beeps?", default=AUDIO_BEEPS)
    console.print(f"[green]Settings updated: Speaker={SPEAKER_ID}, Duration={SAMPLE_DURATION}s, Beeps={AUDIO_BEEPS}[/green]\n")


def main():
    ensure_dataset_dirs()

    header = Panel(
        "[bold cyan]🎙 TinyML Voice Dataset Collector[/bold cyan]\n"
        "[white]Hardware target: Arduino Nano 33 BLE Sense (16kHz, 16-bit Mono WAV)[/white]\n"
        f"[dim]Dataset directory: {DATASET_DIR}[/dim]",
        border_style="cyan"
    )

    while True:
        console.print(header)
        console.print("[bold]Main Menu:[/bold]")
        console.print("  [bold cyan]1[/bold cyan]. ⚡ Rapid-Fire / Batch Mode [italic green](Record 10-50 samples quickly)[/italic green]")
        console.print("  [bold cyan]2[/bold cyan]. 🎙 Single Sample Mode [dim](Record with instant playback & review)[/dim]")
        console.print("  [bold cyan]3[/bold cyan]. 🌊 Background Noise Collector [dim](Record continuous ambient audio)[/dim]")
        console.print("  [bold cyan]4[/bold cyan]. 📊 View Dataset Statistics & Counts")
        console.print("  [bold cyan]5[/bold cyan]. 🔊 Test Microphone & Live Volume Meter")
        console.print("  [bold cyan]6[/bold cyan]. 🎧 Change Audio Input Device")
        console.print("  [bold cyan]7[/bold cyan]. ⚙ Settings [dim](Speaker ID, Clip Duration, Beeps)[/dim]")
        console.print("  [bold cyan]0[/bold cyan]. 🚪 Exit")

        choice = Prompt.ask("\nEnter choice", choices=["1", "2", "3", "4", "5", "6", "7", "0"], default="1")

        if choice == "1":
            rapid_fire_batch_mode()
        elif choice == "2":
            single_record_mode()
        elif choice == "3":
            background_noise_mode()
        elif choice == "4":
            show_dataset_stats()
        elif choice == "5":
            test_microphone()
        elif choice == "6":
            list_audio_devices()
        elif choice == "7":
            settings_menu()
        elif choice == "0":
            console.print("[bold green]Happy Training! Exiting...[/bold green]")
            break

        console.print("\n" + "=" * 50 + "\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        console.print("\n[yellow]Interrupted by user. Bye![/yellow]")
