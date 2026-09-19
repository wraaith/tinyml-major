# Smart Home BLE Controller — Flutter App Architecture

## Project Structure

```
mobile-app/
├── pubspec.yaml                  # Dependencies: flutter_blue_plus, provider, google_fonts
├── analysis_options.yaml
├── android_permissions.xml       # BLE permissions to merge into AndroidManifest.xml
└── lib/
    ├── main.dart                 # Entry point, Provider setup, screen routing
    ├── theme/
    │   └── app_theme.dart        # Dark-mode palette, per-appliance accent colours
    ├── models/
    │   └── appliance.dart        # Appliance & Scene data models
    ├── services/
    │   └── ble_service.dart      # Core BLE logic (NUS connection, send/receive, state)
    ├── screens/
    │   ├── connection_screen.dart # Animated scan/connect screen
    │   └── dashboard_screen.dart  # Main control dashboard
    └── widgets/
        ├── appliance_card.dart   # Glassmorphism card with toggle + slider
        ├── scene_button.dart     # Scene preset button
        ├── emergency_button.dart # Full-width emergency stop
        └── ble_terminal.dart     # Log viewer + manual command input
```

## Firmware ↔ App Protocol

The app communicates with your Arduino Nano 33 BLE Sense via the **Nordic UART Service (NUS)**:

| UUID | Direction | Purpose |
|------|-----------|---------|
| `6E400001-...` | — | NUS Service |
| `6E400002-...` | Phone → Board (Write) | Commands |
| `6E400003-...` | Board → Phone (Notify) | Responses |

### Commands sent by the app

| Command | Example | Action |
|---------|---------|--------|
| `ON:<id>` | `ON:1\n` | Turn ON appliance |
| `OFF:<id>` | `OFF:2\n` | Turn OFF appliance |
| `TOGGLE:<id>` | `TOGGLE:3\n` | Toggle state |
| `DIM:<id>,<pct>` | `DIM:1,50\n` | Set intensity |
| `SCENE:<id>` | `SCENE:2\n` | Activate preset |
| `STATUS` | `STATUS\n` | Request all states |
| `EMERGENCY` | `EMERGENCY\n` | Kill all outputs |

### Responses parsed by the app

```
Appliance 1: ON (100%)
Appliance 2: OFF (0%)
OK
ERROR: Command rejected by safety manager
```

## Setup Instructions

### 1. Install Flutter SDK
Download from [flutter.dev](https://flutter.dev/docs/get-started/install/windows)

### 2. Scaffold the Flutter project
```bash
cd "c:\Users\preet\code projts\tinyml-major\smart-home\mobile-app"
flutter create . --org com.smarthome --project-name smart_home_ble
```
> This generates `android/`, `ios/`, `test/`, `web/` etc. around the existing `lib/` and `pubspec.yaml`.

### 3. Add Android BLE permissions
Open `android/app/src/main/AndroidManifest.xml` and add the contents of `android_permissions.xml` inside the `<manifest>` tag, **before** the `<application>` tag.

### 4. Install dependencies
```bash
flutter pub get
```

### 5. Run on device
```bash
flutter run
```

> [!IMPORTANT]
> BLE requires a **physical device** — it does not work in the Android emulator. Connect your phone via USB and enable USB debugging.

## Key Design Decisions

1. **`flutter_blue_plus`** over `flutter_reactive_ble`: More actively maintained, simpler API, better Android 12+ permission handling.

2. **Optimistic UI updates**: Toggle/slider changes reflect immediately in the UI, then send the BLE command. If the device responds with different state (via STATUS), it corrects.

3. **MTU chunking**: Messages > 20 bytes are split into chunks with 15ms delay between them, matching the firmware's `delay(10)` in `ble_serial.h`.

4. **Provider for state**: Single `BleService` ChangeNotifier manages connection state, appliance state, and log — all screens reactively rebuild.
