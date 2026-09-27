#!/usr/bin/env python3
"""
TinyML Voice Studio (Desktop Application)
=========================================
A dedicated desktop application for recording and managing keyword audio datasets
for microcontrollers (Arduino Nano 33 BLE Sense Rev2).

Specifications:
- 16 kHz, 16-bit Mono PCM WAV
- Single & Rapid-Fire Batch Recording modes
- Spacebar shortcut for instant recording
- Instant playback & delete-last-sample
- Live volume & clipping analysis
- One-click dataset folder explorer & train/val/test export
"""

import os
import sys
import time
import json
import wave
import random
import threading
import datetime
import winsound
import numpy as np
import sounddevice as sd
import customtkinter as ctk

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

# App Appearance
ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")


class TinyMLVoiceStudio(ctk.CTk):
    def __init__(self):
        super().__init__()
        ensure_dataset_dirs()

        # Window Setup
        self.title("TinyML Voice Studio - Keyword Dataset Collector")
        self.geometry("860x720")
        self.minsize(800, 640)

        # State
        self.sample_duration = DEFAULT_DURATION
        self.current_device_idx = None
        self.device_map = {}
        self.last_audio = None
        self.last_file_path = None
        self.is_recording = False
        self.batch_running = False
        self.cancel_batch = False

        self._build_ui()
        self._load_audio_devices()
        self._refresh_stats()

        # Bind Spacebar to Record Single (only when focus is NOT on a text entry)
        self.bind("<space>", self._on_space_pressed)

    def _build_ui(self):
        # Top Header Bar
        header = ctk.CTkFrame(self, corner_radius=12, fg_color="#1e293b")
        header.pack(fill="x", padx=16, pady=(16, 8))

        top_row = ctk.CTkFrame(header, fg_color="transparent")
        top_row.pack(fill="x", padx=16, pady=(12, 4))

        title = ctk.CTkLabel(
            top_row,
            text="🎙 TinyML Voice Studio",
            font=ctk.CTkFont(size=22, weight="bold"),
            text_color="#38bdf8"
        )
        title.pack(side="left")

        # Open Folder & Export buttons on top right
        btn_box = ctk.CTkFrame(top_row, fg_color="transparent")
        btn_box.pack(side="right")

        export_btn = ctk.CTkButton(
            btn_box,
            text="📊 Export Splits",
            width=110,
            height=32,
            fg_color="#334155",
            hover_color="#475569",
            command=self._export_splits
        )
        export_btn.pack(side="left", padx=4)

        folder_btn = ctk.CTkButton(
            btn_box,
            text="📂 Open Folder",
            width=110,
            height=32,
            fg_color="#0284c7",
            hover_color="#0369a1",
            command=self._open_dataset_folder
        )
        folder_btn.pack(side="left", padx=4)

        subtitle = ctk.CTkLabel(
            header,
            text=f"Target: 16 kHz Mono WAV (16-bit PCM) for Arduino Nano 33 BLE Sense | Spacebar = Record",
            font=ctk.CTkFont(size=12),
            text_color="#94a3b8"
        )
        subtitle.pack(anchor="w", padx=16, pady=(0, 12))

        # Main 2-Column Layout
        content = ctk.CTkFrame(self, fg_color="transparent")
        content.pack(fill="both", expand=True, padx=16, pady=8)

        # Left Column: Recording Controls
        left_col = ctk.CTkFrame(content, corner_radius=12)
        left_col.pack(side="left", fill="both", expand=True, padx=(0, 8))

        # 1. Target Class Selection
        ctk.CTkLabel(
            left_col,
            text="Target Command Word / Class",
            font=ctk.CTkFont(size=14, weight="bold")
        ).pack(anchor="w", padx=16, pady=(14, 4))

        self.class_var = ctk.StringVar(value=DEFAULT_CLASSES[0])
        self.class_menu = ctk.CTkOptionMenu(
            left_col,
            values=self._get_all_classes(),
            variable=self.class_var,
            command=self._on_class_change,
            height=36,
            font=ctk.CTkFont(size=13, weight="bold")
        )
        self.class_menu.pack(fill="x", padx=16, pady=(0, 8))

        # Add custom class inline
        add_frame = ctk.CTkFrame(left_col, fg_color="transparent")
        add_frame.pack(fill="x", padx=16, pady=(0, 10))

        self.custom_entry = ctk.CTkEntry(
            add_frame,
            placeholder_text="Add custom word (e.g. bedroom_light)",
            height=32
        )
        self.custom_entry.pack(side="left", fill="x", expand=True, padx=(0, 6))

        add_btn = ctk.CTkButton(
            add_frame,
            text="+ Add",
            width=65,
            height=32,
            fg_color="#059669",
            hover_color="#047857",
            command=self._add_custom_class
        )
        add_btn.pack(side="right")

        # 2. Hardware & Speaker Settings
        cfg_frame = ctk.CTkFrame(left_col, fg_color="#1e293b", corner_radius=8)
        cfg_frame.pack(fill="x", padx=16, pady=(0, 12))

        # Mic Device row
        ctk.CTkLabel(cfg_frame, text="Microphone:", font=ctk.CTkFont(size=12)).grid(row=0, column=0, sticky="w", padx=10, pady=6)
        self.device_menu = ctk.CTkOptionMenu(
            cfg_frame,
            values=["Default Microphone"],
            command=self._on_device_change,
            width=200,
            height=28
        )
        self.device_menu.grid(row=0, column=1, sticky="ew", padx=10, pady=6)

        # Speaker ID & Duration row
        ctk.CTkLabel(cfg_frame, text="Speaker Tag:", font=ctk.CTkFont(size=12)).grid(row=1, column=0, sticky="w", padx=10, pady=6)
        sub_row = ctk.CTkFrame(cfg_frame, fg_color="transparent")
        sub_row.grid(row=1, column=1, sticky="w", padx=10, pady=6)

        self.speaker_entry = ctk.CTkEntry(sub_row, width=90, height=28)
        self.speaker_entry.insert(0, "speaker1")
        self.speaker_entry.pack(side="left", padx=(0, 8))

        ctk.CTkLabel(sub_row, text="Duration:", font=ctk.CTkFont(size=12)).pack(side="left", padx=(0, 4))
        self.duration_var = ctk.StringVar(value="1.0s")
        self.duration_menu = ctk.CTkOptionMenu(
            sub_row,
            values=["0.8s", "1.0s", "1.2s", "1.5s", "2.0s"],
            variable=self.duration_var,
            width=80,
            height=28,
            command=self._on_duration_change
        )
        self.duration_menu.pack(side="left")

        # 3. Status Display & Progress
        status_box = ctk.CTkFrame(left_col, fg_color="transparent")
        status_box.pack(fill="x", padx=16, pady=4)

        self.status_label = ctk.CTkLabel(
            status_box,
            text="Ready (Press Space or Click Below)",
            font=ctk.CTkFont(size=16, weight="bold"),
            text_color="#38bdf8"
        )
        self.status_label.pack(pady=(4, 4))

        self.progress_bar = ctk.CTkProgressBar(status_box, height=12)
        self.progress_bar.set(0)
        self.progress_bar.pack(fill="x", pady=(0, 8))

        # 4. Action Buttons
        self.record_single_btn = ctk.CTkButton(
            left_col,
            text="🔴 RECORD SINGLE SAMPLE (SPACEBAR)",
            font=ctk.CTkFont(size=14, weight="bold"),
            fg_color="#dc2626",
            hover_color="#b91c1c",
            height=46,
            command=self._start_single_record
        )
        self.record_single_btn.pack(fill="x", padx=16, pady=(0, 6))

        # Batch record row
        batch_row = ctk.CTkFrame(left_col, fg_color="transparent")
        batch_row.pack(fill="x", padx=16, pady=(0, 8))

        self.batch_btn = ctk.CTkButton(
            batch_row,
            text="⚡ Rapid-Fire Batch (10x Samples)",
            font=ctk.CTkFont(size=13, weight="bold"),
            fg_color="#0284c7",
            hover_color="#0369a1",
            height=38,
            command=self._start_batch_record
        )
        self.batch_btn.pack(side="left", fill="x", expand=True, padx=(0, 6))

        self.batch_count_menu = ctk.CTkOptionMenu(
            batch_row,
            values=["5x", "10x", "20x", "30x", "50x"],
            width=75,
            height=38,
            command=self._on_batch_count_change
        )
        self.batch_count_menu.set("10x")
        self.batch_count_menu.pack(side="right")

        # 5. Playback & Review Controls
        review_frame = ctk.CTkFrame(left_col, fg_color="#1e293b", corner_radius=8)
        review_frame.pack(fill="x", padx=16, pady=(0, 10))

        review_btns = ctk.CTkFrame(review_frame, fg_color="transparent")
        review_btns.pack(fill="x", padx=10, pady=(8, 4))

        self.play_btn = ctk.CTkButton(
            review_btns,
            text="▶ Listen to Last",
            height=32,
            fg_color="#334155",
            hover_color="#475569",
            command=self._play_last_audio,
            state="disabled"
        )
        self.play_btn.pack(side="left", fill="x", expand=True, padx=(0, 4))

        self.delete_btn = ctk.CTkButton(
            review_btns,
            text="🗑 Delete Last",
            height=32,
            width=100,
            fg_color="#7f1d1d",
            hover_color="#991b1b",
            command=self._delete_last_sample,
            state="disabled"
        )
        self.delete_btn.pack(side="right")

        self.audio_stats_lbl = ctk.CTkLabel(
            review_frame,
            text="Audio info: No sample recorded yet",
            font=ctk.CTkFont(size=11),
            text_color="#94a3b8"
        )
        self.audio_stats_lbl.pack(padx=10, pady=(0, 6))

        # Right Column: Dataset Inventory
        right_col = ctk.CTkFrame(content, corner_radius=12, width=300)
        right_col.pack(side="right", fill="both", expand=False)

        rt_header = ctk.CTkFrame(right_col, fg_color="transparent")
        rt_header.pack(fill="x", padx=14, pady=(14, 6))

        ctk.CTkLabel(
            rt_header,
            text="📊 Dataset Inventory",
            font=ctk.CTkFont(size=15, weight="bold")
        ).pack(side="left")

        refresh_btn = ctk.CTkButton(
            rt_header,
            text="⟳",
            width=30,
            height=26,
            fg_color="#334155",
            hover_color="#475569",
            command=self._refresh_stats
        )
        refresh_btn.pack(side="right")

        self.stats_scroll = ctk.CTkScrollableFrame(right_col, width=280)
        self.stats_scroll.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        # Inventory Footer
        self.total_lbl = ctk.CTkLabel(
            right_col,
            text="Total: 0 samples",
            font=ctk.CTkFont(size=13, weight="bold"),
            text_color="#10b981"
        )
        self.total_lbl.pack(pady=(4, 12))

    def _load_audio_devices(self):
        try:
            devs = sd.query_devices()
            device_names = []
            self.device_map = {}
            for i, d in enumerate(devs):
                if d['max_input_channels'] > 0:
                    name = f"[{i}] {d['name'][:28]}"
                    device_names.append(name)
                    self.device_map[name] = i

            if device_names:
                self.device_menu.configure(values=device_names)
                self.device_menu.set(device_names[0])
                self.current_device_idx = self.device_map[device_names[0]]
        except Exception:
            pass

    def _on_device_change(self, val):
        self.current_device_idx = self.device_map.get(val, None)

    def _on_space_pressed(self, event=None):
        # Don't trigger recording when user is typing in an Entry widget
        if event and isinstance(event.widget, (ctk.CTkEntry,)):
            return
        try:
            focused = self.focus_get()
            if focused and isinstance(focused, (ctk.CTkEntry,)):
                return
        except Exception:
            pass
        if not self.is_recording and not self.batch_running:
            self._start_single_record()

    def _get_all_classes(self):
        classes = set(DEFAULT_CLASSES)
        if os.path.exists(DATASET_DIR):
            for d in os.listdir(DATASET_DIR):
                if os.path.isdir(os.path.join(DATASET_DIR, d)):
                    classes.add(d)
        return sorted(list(classes))

    def _on_class_change(self, val):
        self._refresh_stats()

    def _on_duration_change(self, val):
        self.sample_duration = float(val.replace("s", ""))

    def _on_batch_count_change(self, val):
        self.batch_btn.configure(text=f"⚡ Rapid-Fire Batch ({val} Samples)")

    def _add_custom_class(self):
        name = self.custom_entry.get().strip().lower()
        name = "".join(c if c.isalnum() or c == '_' else '_' for c in name)
        if name:
            os.makedirs(os.path.join(DATASET_DIR, name), exist_ok=True)
            all_classes = self._get_all_classes()
            self.class_menu.configure(values=all_classes)
            self.class_var.set(name)
            self.custom_entry.delete(0, "end")
            self._refresh_stats()

    def _refresh_stats(self):
        for child in self.stats_scroll.winfo_children():
            child.destroy()

        total = 0
        current_sel = self.class_var.get()

        for cls in self._get_all_classes():
            folder = os.path.join(DATASET_DIR, cls)
            count = len([f for f in os.listdir(folder) if f.endswith(".wav")]) if os.path.exists(folder) else 0
            total += count

            is_selected = (cls == current_sel)
            item_frame = ctk.CTkFrame(
                self.stats_scroll,
                fg_color="#1e293b" if is_selected else "transparent",
                corner_radius=6,
                height=30
            )
            item_frame.pack(fill="x", pady=2)

            name_btn = ctk.CTkButton(
                item_frame,
                text=f"{cls}",
                font=ctk.CTkFont(size=12, weight="bold" if is_selected else "normal"),
                text_color="#38bdf8" if is_selected else "white",
                fg_color="transparent",
                hover_color="#334155",
                anchor="w",
                command=lambda c=cls: self._select_class(c)
            )
            name_btn.pack(side="left", fill="x", expand=True, padx=4)

            # Badge color coding
            badge_color = "#10b981" if count >= 30 else ("#f59e0b" if count > 0 else "#64748b")
            count_badge = ctk.CTkLabel(
                item_frame,
                text=f"{count}",
                font=ctk.CTkFont(size=11, weight="bold"),
                fg_color=badge_color,
                corner_radius=6,
                width=34,
                height=22
            )
            count_badge.pack(side="right", padx=6)

        self.total_lbl.configure(text=f"Total: {total} clips ({(total * self.sample_duration):.1f}s)")

    def _select_class(self, cls_name):
        self.class_var.set(cls_name)
        self._refresh_stats()

    def _start_single_record(self):
        if self.is_recording or self.batch_running:
            return
        self.is_recording = True
        self.record_single_btn.configure(state="disabled")
        self.batch_btn.configure(state="disabled")
        threading.Thread(target=self._single_record_worker, daemon=True).start()

    def _single_record_worker(self):

        # Visual 3.. 2.. 1.. Countdown
        for i in range(3, 0, -1):
            self.status_label.configure(text=f"Get Ready... {i}", text_color="#f59e0b")
            try:
                winsound.Beep(600, 80)
            except Exception:
                pass
            time.sleep(0.5)

        # Audio recording starts
        target_word = self.class_var.get().upper()
        self.status_label.configure(text=f"🔴 SPEAK NOW: '{target_word}'", text_color="#ef4444")
        try:
            winsound.Beep(1200, 150)
        except Exception:
            pass

        samples_total = int(self.sample_duration * SAMPLE_RATE)
        recording = sd.rec(
            samples_total,
            samplerate=SAMPLE_RATE,
            channels=CHANNELS,
            dtype=DTYPE,
            device=self.current_device_idx
        )

        # Smooth progress bar
        steps = 20
        step_time = self.sample_duration / steps
        for step in range(steps):
            self.progress_bar.set((step + 1) / steps)
            time.sleep(step_time)

        sd.wait()
        try:
            winsound.Beep(400, 100)
        except Exception:
            pass

        audio = recording.squeeze()
        self.last_audio = audio

        rms, peak, is_silent, is_clipped = analyze_audio(audio)

        # Save to WAV
        cls = self.class_var.get()
        speaker = self.speaker_entry.get().strip() or "speaker1"
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{speaker}_{cls}_{ts}.wav"
        filepath = os.path.join(DATASET_DIR, cls, filename)
        save_wav(filepath, audio, SAMPLE_RATE)
        self.last_file_path = filepath

        # Update UI
        self.progress_bar.set(1.0)
        self.status_label.configure(text=f"✔ Saved: {filename}", text_color="#10b981")

        info_text = f"RMS: {rms:.0f} | Peak: {peak:.0f}"
        if is_silent:
            info_text += " [Silent - Check Mic!]"
        if is_clipped:
            info_text += " [Clipped - Speak softer!]"
        self.audio_stats_lbl.configure(text=info_text)

        self.play_btn.configure(state="normal")
        self.delete_btn.configure(state="normal")
        self._refresh_stats()

        self.is_recording = False
        self.record_single_btn.configure(state="normal")
        self.batch_btn.configure(state="normal")

    def _start_batch_record(self):
        if self.is_recording or self.batch_running:
            return
        self.batch_running = True
        self.cancel_batch = False
        self.record_single_btn.configure(state="disabled")
        self.batch_btn.configure(
            text="🛑 Stop Batch",
            fg_color="#ef4444",
            hover_color="#b91c1c",
            command=self._stop_batch
        )
        threading.Thread(target=self._batch_record_worker, daemon=True).start()

    def _stop_batch(self):
        self.cancel_batch = True
        self.batch_btn.configure(state="disabled", text="Stopping...")

    def _batch_record_worker(self):

        count_str = self.batch_count_menu.get().replace("x", "")
        count = int(count_str)
        cls = self.class_var.get()
        speaker = self.speaker_entry.get().strip() or "speaker1"

        for i in range(1, count + 1):
            if self.cancel_batch:
                self.status_label.configure(text=f"⏹ Batch Stopped ({i-1}/{count} samples)", text_color="#f59e0b")
                break
                
            self.status_label.configure(
                text=f"[{i}/{count}] Ready in 1s...",
                text_color="#f59e0b"
            )
            time.sleep(0.4)
            try:
                winsound.Beep(700, 80)
            except Exception:
                pass
            time.sleep(0.4)
            try:
                winsound.Beep(1200, 150)
            except Exception:
                pass

            self.status_label.configure(
                text=f"🔴 [{i}/{count}] SAY: '{cls.upper()}'",
                text_color="#ef4444"
            )

            samples_total = int(self.sample_duration * SAMPLE_RATE)
            recording = sd.rec(
                samples_total,
                samplerate=SAMPLE_RATE,
                channels=CHANNELS,
                dtype=DTYPE,
                device=self.current_device_idx
            )

            steps = 10
            for step in range(steps):
                self.progress_bar.set((step + 1) / steps)
                time.sleep(self.sample_duration / steps)

            sd.wait()
            try:
                winsound.Beep(400, 80)
            except Exception:
                pass

            audio = recording.squeeze()
            self.last_audio = audio

            rms, peak, is_silent, is_clipped = analyze_audio(audio)
            
            ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{speaker}_{cls}_{ts}_{i:03d}.wav"
            filepath = os.path.join(DATASET_DIR, cls, filename)
            save_wav(filepath, audio, SAMPLE_RATE)
            self.last_file_path = filepath

            self._refresh_stats()
            time.sleep(0.9)  # Breather between repetitions

        if not self.cancel_batch:
            self.status_label.configure(text=f"🎉 Batch Finished ({count} samples)!", text_color="#10b981")
        self.play_btn.configure(state="normal")
        self.delete_btn.configure(state="normal")
        self.batch_running = False
        self.record_single_btn.configure(state="normal")
        
        val = self.batch_count_menu.get()
        self.batch_btn.configure(
            text=f"⚡ Rapid-Fire Batch ({val} Samples)",
            fg_color="#0284c7",
            hover_color="#0369a1",
            command=self._start_batch_record,
            state="normal"
        )

    def _play_last_audio(self):
        if self.last_audio is not None:
            threading.Thread(target=lambda: sd.play(self.last_audio, SAMPLE_RATE), daemon=True).start()

    def _delete_last_sample(self):
        if self.last_file_path and os.path.exists(self.last_file_path):
            try:
                os.remove(self.last_file_path)
                self.status_label.configure(text="🗑 Deleted last sample", text_color="#f59e0b")
                self.last_file_path = None
                self.delete_btn.configure(state="disabled")
                self._refresh_stats()
            except Exception as e:
                self.status_label.configure(text=f"Error deleting: {e}", text_color="#ef4444")

    def _open_dataset_folder(self):
        try:
            os.startfile(DATASET_DIR)
        except Exception:
            os.system(f'explorer "{DATASET_DIR}"')

    def _export_splits(self):
        """Export labels.txt and dataset_splits.json without requiring a console."""
        import wave as _wave
        try:
            ensure_dataset_dirs()
            # Find classes with valid WAV files
            all_valid = {}
            classes = sorted([d for d in os.listdir(DATASET_DIR)
                              if os.path.isdir(os.path.join(DATASET_DIR, d))])
            for cls in classes:
                cls_dir = os.path.join(DATASET_DIR, cls)
                valid = []
                for f in os.listdir(cls_dir):
                    if f.endswith(".wav"):
                        fp = os.path.join(cls_dir, f)
                        try:
                            with _wave.open(fp, 'rb') as wf:
                                if wf.getnframes() > 0:
                                    valid.append(fp)
                        except Exception:
                            pass
                all_valid[cls] = valid

            # Write labels.txt
            active = [c for c, flist in all_valid.items() if flist]
            with open(os.path.join(DATASET_DIR, "labels.txt"), "w") as f:
                for c in active:
                    f.write(f"{c}\n")

            # Write train/val/test splits
            import random as _rand
            train, val, test = [], [], []
            for cls, files in all_valid.items():
                if not files:
                    continue
                shuffled = files.copy()
                _rand.seed(42)
                _rand.shuffle(shuffled)
                n = len(shuffled)
                nt = int(n * 0.8)
                nv = int(n * 0.1)
                train.extend(shuffled[:nt])
                val.extend(shuffled[nt:nt + nv])
                test.extend(shuffled[nt + nv:])

            splits = {
                "train_count": len(train), "val_count": len(val), "test_count": len(test),
                "train": [os.path.relpath(p, DATASET_DIR).replace("\\", "/") for p in train],
                "val":   [os.path.relpath(p, DATASET_DIR).replace("\\", "/") for p in val],
                "test":  [os.path.relpath(p, DATASET_DIR).replace("\\", "/") for p in test],
            }
            with open(os.path.join(DATASET_DIR, "dataset_splits.json"), "w") as f:
                json.dump(splits, f, indent=2)

            self.status_label.configure(
                text=f"Exported {len(active)} labels + splits ({len(train)}/{len(val)}/{len(test)})!",
                text_color="#10b981"
            )
        except Exception as e:
            self.status_label.configure(text=f"Export error: {e}", text_color="#ef4444")


if __name__ == "__main__":
    app = TinyMLVoiceStudio()
    app.mainloop()
