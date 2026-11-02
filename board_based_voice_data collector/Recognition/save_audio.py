#!/usr/bin/env python3
"""
save_audio.py — Companion PC script for the Arduino Nano 33 BLE Sense Rev2
audio capture sketch.

Fully interactive:
  1. Choose frequency and sample length (like Edge Impulse)
  2. Enter a label name → a folder is created for it
  3. Press Enter to record clips → saved into that label folder
  4. Type 'q' to finish that label and pick a new one
  5. Type 'exit' to quit

Usage
-----
  python save_audio.py --port COM5
  python save_audio.py --port COM5 --out ./dataset
"""

import argparse
import os
import sys
import time
import wave

import serial  # pip install pyserial


def wait_for_line(ser, prefix, timeout=30.0):
    """Read lines until one starts with *prefix*; return that line."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()
        if line:
            print(f"  [BOARD] {line}")
        if line.startswith(prefix):
            return line
    raise TimeoutError(f"Timed out waiting for '{prefix}'")


def read_exact(ser, n, timeout=10.0):
    """Read exactly *n* bytes from the serial port."""
    buf = b""
    deadline = time.time() + timeout
    while len(buf) < n and time.time() < deadline:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf += chunk
    if len(buf) != n:
        raise TimeoutError(f"Expected {n} bytes, got {len(buf)}")
    return buf


def save_wav(path, pcm_data, sample_rate, bits, channels=1):
    """Write raw PCM bytes to a .wav file."""
    with wave.open(path, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(bits // 8)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm_data)
    print(f"  Saved: {path}  ({os.path.getsize(path)} bytes)")


def drain_serial(ser):
    """Read and print any pending serial data."""
    while ser.in_waiting:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if line:
            print(f"  [BOARD] {line}")


def configure_board(ser, sample_rate, duration_ms):
    """Send CONFIG command to board and wait for confirmation."""
    cmd = f"CONFIG {sample_rate} {duration_ms}\n"
    ser.write(cmd.encode())
    line = wait_for_line(ser, "CONFIG", timeout=10)
    if line.startswith("CONFIG_OK"):
        parts = line.split()
        actual_rate    = int(parts[1])
        actual_len     = int(parts[2])
        actual_samples = int(parts[3])
        print(f"\n  Board configured:")
        print(f"    Frequency    : {actual_rate} Hz")
        print(f"    Duration     : {actual_len} ms")
        print(f"    Total samples: {actual_samples}")
        return True
    else:
        print(f"  ERROR: {line}")
        return False


def record_clip(ser, label_folder, label, clip_num):
    """Trigger one recording and save the .wav file."""
    ser.write(b"r\n")

    wait_for_line(ser, "REC_START")

    header_line = wait_for_line(ser, "AUDIO_START")
    parts = header_line.split()
    num_samples     = int(parts[1])
    sample_rate     = int(parts[2])
    bits_per_sample = int(parts[3])
    num_bytes = num_samples * (bits_per_sample // 8)

    print(f"  Recording... receiving {num_samples} samples ({num_bytes} bytes)")
    pcm_data = read_exact(ser, num_bytes, timeout=30)

    wait_for_line(ser, "AUDIO_END")

    timestamp = int(time.time() * 1000)
    filename = f"{label}_{clip_num:04d}_{timestamp}.wav"
    filepath = os.path.join(label_folder, filename)

    save_wav(filepath, pcm_data, sample_rate, bits_per_sample)
    return filepath


def ask_frequency():
    """Ask user to pick a sample rate."""
    print("\n  Select frequency (sample rate):")
    print("    1) 8000 Hz")
    print("    2) 11025 Hz")
    print("    3) 16000 Hz  [default - Edge Impulse recommended]")
    print("    4) 22050 Hz")
    print("    5) 41000 Hz")
    print("    6) Custom")
    choice = input("  Choice [3]: ").strip()

    rates = {"1": 8000, "2": 11025, "3": 16000, "4": 22050, "5": 41000}

    if choice in rates:
        return rates[choice]
    elif choice == "6":
        val = int(input("  Enter frequency in Hz (8000-41000): ").strip())
        return max(8000, min(41000, val))
    else:
        return 16000  # default


def ask_duration():
    """Ask user to pick a sample length."""
    print("\n  Select sample length:")
    print("    1) 500 ms")
    print("    2) 1000 ms (1 sec)  [default - Edge Impulse recommended]")
    print("    3) 1500 ms")
    print("    4) 2000 ms (2 sec)")
    print("    5) 3000 ms (3 sec)")
    print("    6) Custom")
    choice = input("  Choice [2]: ").strip()

    lengths = {"1": 500, "2": 1000, "3": 1500, "4": 2000, "5": 3000}

    if choice in lengths:
        return lengths[choice]
    elif choice == "6":
        val = int(input("  Enter duration in ms (100-30000): ").strip())
        return max(100, min(30000, val))
    else:
        return 1000  # default


def main():
    parser = argparse.ArgumentParser(description="Save audio from Nano 33 BLE Sense Rev2 for Edge Impulse")
    parser.add_argument("--port", required=True, help="Serial port (e.g. COM5)")
    parser.add_argument("--out", default="./edge_impulse_data", help="Root output folder")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    print(f"\nOpening {args.port} at {args.baud} baud ...")
    ser = serial.Serial(args.port, args.baud, timeout=2)
    time.sleep(2)

    # Wait for READY from board
    try:
        wait_for_line(ser, "READY", timeout=10)
    except TimeoutError:
        drain_serial(ser)
        print("  Warning: Did not receive READY from board. Continuing anyway...")

    print("\n" + "=" * 48)
    print("   Edge Impulse Audio Data Collector")
    print("   Arduino Nano 33 BLE Sense Rev2")
    print("=" * 48)

    # --- Ask for frequency and duration ---
    sample_rate = ask_frequency()
    duration_ms = ask_duration()

    print(f"\n  Configuring board: {sample_rate} Hz, {duration_ms} ms ...")
    if not configure_board(ser, sample_rate, duration_ms):
        print("  Failed to configure board. Exiting.")
        ser.close()
        return

    # --- Main label loop ---
    while True:
        print("\n" + "-" * 48)
        label = input("  Enter label name (or 'exit' to quit): ").strip()

        if not label:
            print("  Label cannot be empty. Try again.")
            continue

        if label.lower() == "exit":
            print("\n  Goodbye!")
            break

        if label.lower() == "config":
            # Let user reconfigure frequency/duration
            sample_rate = ask_frequency()
            duration_ms = ask_duration()
            print(f"\n  Reconfiguring board: {sample_rate} Hz, {duration_ms} ms ...")
            configure_board(ser, sample_rate, duration_ms)
            continue

        # Create folder for this label
        label_folder = os.path.join(args.out, label)
        os.makedirs(label_folder, exist_ok=True)

        # Count existing files to continue numbering
        existing = [f for f in os.listdir(label_folder) if f.endswith(".wav")]
        clip_num = len(existing)

        print(f"\n  Folder  : {os.path.abspath(label_folder)}")
        print(f"  Settings: {sample_rate} Hz, {duration_ms} ms")
        print(f"  Existing: {clip_num} clips")
        print(f"\n  Press ENTER  = record a clip")
        print(f"  Type 'q'     = finish this label")
        print(f"  Type 'config' = change frequency/duration\n")

        while True:
            user_input = input(f"  [{label}] clip #{clip_num + 1} > ").strip().lower()

            if user_input == "q":
                print(f"\n  Done with '{label}' - {clip_num} total clips in folder.\n")
                break

            if user_input == "config":
                sample_rate = ask_frequency()
                duration_ms = ask_duration()
                print(f"\n  Reconfiguring board: {sample_rate} Hz, {duration_ms} ms ...")
                configure_board(ser, sample_rate, duration_ms)
                print()
                continue

            # Record
            clip_num += 1
            try:
                record_clip(ser, label_folder, label, clip_num)
            except TimeoutError as e:
                print(f"  ERROR: {e}")
                print("  Make sure the board is connected and the sketch is uploaded.")
                clip_num -= 1

            time.sleep(0.3)

    print(f"\n  All data saved under: {os.path.abspath(args.out)}")
    print("  Upload to Edge Impulse Studio -> Data acquisition -> Upload data.")
    ser.close()


if __name__ == "__main__":
    main()
