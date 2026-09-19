import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../models/appliance.dart';

/// BLE service that manages the connection to the Arduino Nano 33 BLE Sense
/// running the SmartHomeCtrl firmware over the Nordic UART Service (NUS).
///
/// Exposes reactive state via [ChangeNotifier] for Provider consumption.
class BleService extends ChangeNotifier {
  // ── NUS UUIDs (match firmware/ble_serial.h) ────────────────────────────
  static final Guid _nusServiceUuid =
      Guid('6E400001-B5A3-F393-E0A9-E50E24DCCA9E');
  static final Guid _nusRxUuid = // Write (phone → board)
      Guid('6E400002-B5A3-F393-E0A9-E50E24DCCA9E');
  static final Guid _nusTxUuid = // Notify (board → phone)
      Guid('6E400003-B5A3-F393-E0A9-E50E24DCCA9E');

  // ── Connection state ───────────────────────────────────────────────────
  BluetoothDevice? _device;
  BluetoothCharacteristic? _rxChar;
  BluetoothCharacteristic? _txChar;

  bool _connected = false;
  bool _scanning = false;
  String _deviceName = '';
  String _statusMessage = 'Tap to scan for SmartHomeCtrl';

  bool get isConnected => _connected;
  bool get isScanning => _scanning;
  String get deviceName => _deviceName;
  String get statusMessage => _statusMessage;

  // ── Appliance state ────────────────────────────────────────────────────
  final List<Appliance> appliances = [
    Appliance(id: 1, name: 'Light',  icon: 'light'),
    Appliance(id: 2, name: 'Fan',    icon: 'fan'),
    Appliance(id: 3, name: 'Heater', icon: 'heater'),
    Appliance(id: 4, name: 'Pump',   icon: 'pump'),
  ];

  int? activeScene;

  // ── Log ────────────────────────────────────────────────────────────────
  final List<LogEntry> logEntries = [];
  static const int _maxLogEntries = 200;

  // ── Internal ───────────────────────────────────────────────────────────
  StreamSubscription? _connectionSub;
  StreamSubscription? _notifySub;
  String _rxBuffer = '';

  // ═════════════════════════════════════════════════════════════════════════
  // SCANNING & CONNECTION
  // ═════════════════════════════════════════════════════════════════════════

  /// Start scanning and attempt connection to SmartHomeCtrl.
  Future<void> scanAndConnect() async {
    if (_scanning) return;

    _scanning = true;
    _statusMessage = 'Scanning for devices…';
    notifyListeners();

    try {
      // Ensure Bluetooth is on
      final adapterState = await FlutterBluePlus.adapterState.first;
      if (adapterState != BluetoothAdapterState.on) {
        _statusMessage = 'Bluetooth is off. Please enable it.';
        _scanning = false;
        notifyListeners();
        return;
      }

      // Start scanning with timeout
      await FlutterBluePlus.startScan(
        withNames: ['SmartHomeCtrl'],
        timeout: const Duration(seconds: 10),
      );

      // Listen to scan results
      bool found = false;
      await for (final results in FlutterBluePlus.onScanResults) {
        for (final result in results) {
          if (result.device.platformName.contains('SmartHome')) {
            found = true;
            await FlutterBluePlus.stopScan();
            await _connectToDevice(result.device);
            break;
          }
        }
        if (found) break;
      }

      if (!found) {
        _statusMessage = 'Device not found. Tap to try again.';
      }
    } catch (e) {
      _statusMessage = 'Scan failed: ${e.toString()}';
      _addLog('Scan error: $e', LogType.error);
    }

    _scanning = false;
    notifyListeners();
  }

  /// Connect to a discovered device.
  Future<void> _connectToDevice(BluetoothDevice device) async {
    _statusMessage = 'Connecting…';
    notifyListeners();

    try {
      await device.connect(timeout: const Duration(seconds: 8));
      _device = device;
      _deviceName = device.platformName.isNotEmpty
          ? device.platformName
          : 'SmartHomeCtrl';

      // Listen for disconnection
      _connectionSub = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _onDisconnected();
        }
      });

      // Discover services
      final services = await device.discoverServices();
      BluetoothService? nusService;
      for (final s in services) {
        if (s.uuid == _nusServiceUuid) {
          nusService = s;
          break;
        }
      }

      if (nusService == null) {
        throw Exception('NUS service not found on device');
      }

      // Find characteristics
      for (final c in nusService.characteristics) {
        if (c.uuid == _nusRxUuid) _rxChar = c;
        if (c.uuid == _nusTxUuid) _txChar = c;
      }

      if (_rxChar == null || _txChar == null) {
        throw Exception('NUS characteristics not found');
      }

      // Subscribe to notifications (board → phone)
      await _txChar!.setNotifyValue(true);
      _notifySub = _txChar!.onValueReceived.listen(_onDataReceived);

      _connected = true;
      _statusMessage = 'Connected';
      _addLog('Connected to $_deviceName', LogType.system);
      notifyListeners();

      // Request initial status after short delay
      await Future.delayed(const Duration(milliseconds: 600));
      await sendCommand('STATUS');
    } catch (e) {
      _statusMessage = 'Connection failed: ${e.toString()}';
      _addLog('Connection error: $e', LogType.error);
      await device.disconnect();
      _device = null;
      notifyListeners();
    }
  }

  /// Handle unexpected disconnection.
  void _onDisconnected() {
    _connected = false;
    _device = null;
    _rxChar = null;
    _txChar = null;
    _connectionSub?.cancel();
    _notifySub?.cancel();
    _statusMessage = 'Disconnected. Tap to reconnect.';
    _addLog('Disconnected from device', LogType.error);

    // Reset appliance states
    for (final app in appliances) {
      app.isOn = false;
      app.intensity = 0;
    }
    activeScene = null;

    notifyListeners();
  }

  /// Manually disconnect.
  Future<void> disconnect() async {
    try {
      await _notifySub?.cancel();
      await _connectionSub?.cancel();
      await _device?.disconnect();
    } catch (_) {}
    _onDisconnected();
  }

  // ═════════════════════════════════════════════════════════════════════════
  // DATA SEND / RECEIVE
  // ═════════════════════════════════════════════════════════════════════════

  /// Send a command string to the Arduino. Appends `\n` terminator.
  /// Handles chunking for BLE MTU (20 bytes).
  Future<void> sendCommand(String command) async {
    if (_rxChar == null || !_connected) {
      _addLog('Cannot send — not connected', LogType.error);
      return;
    }

    final msg = '$command\n';
    final data = utf8.encode(msg);
    const mtu = 20;

    _addLog('→ $command', LogType.sent);

    try {
      for (int offset = 0; offset < data.length; offset += mtu) {
        final end = (offset + mtu > data.length) ? data.length : offset + mtu;
        final chunk = Uint8List.fromList(data.sublist(offset, end));
        await _rxChar!.write(chunk, withoutResponse: true);

        if (end < data.length) {
          await Future.delayed(const Duration(milliseconds: 15));
        }
      }
    } catch (e) {
      _addLog('Send error: $e', LogType.error);
    }
  }

  /// Handle incoming BLE notifications.
  void _onDataReceived(List<int> data) {
    final text = utf8.decode(data, allowMalformed: true);
    _rxBuffer += text;

    // Process complete lines
    final lines = _rxBuffer.split(RegExp(r'\r?\n'));
    _rxBuffer = lines.removeLast(); // keep incomplete tail

    for (final line in lines) {
      final trimmed = line.trim();
      if (trimmed.isEmpty) continue;

      _addLog('← $trimmed', LogType.received);
      _parseStatusLine(trimmed);
    }

    notifyListeners();
  }

  /// Parse a status response line from the firmware.
  /// E.g. "Appliance 1: ON (100%)"
  void _parseStatusLine(String line) {
    final match =
        RegExp(r'Appliance\s+(\d+):\s+(ON|OFF)\s+\((\d+)%\)', caseSensitive: false)
            .firstMatch(line);

    if (match != null) {
      final id = int.parse(match.group(1)!);
      final isOn = match.group(2)!.toUpperCase() == 'ON';
      final intensity = int.parse(match.group(3)!);

      if (id >= 1 && id <= 4) {
        final app = appliances[id - 1];
        app.isOn = isOn;
        app.intensity = intensity;
        notifyListeners();
      }
    }
  }

  // ═════════════════════════════════════════════════════════════════════════
  // HIGH-LEVEL COMMANDS
  // ═════════════════════════════════════════════════════════════════════════

  /// Toggle appliance ON/OFF.
  Future<void> toggleAppliance(int id) async {
    final app = appliances[id - 1];
    // Optimistic update
    app.isOn = !app.isOn;
    app.intensity = app.isOn ? 100 : 0;
    activeScene = null;
    notifyListeners();

    await sendCommand('TOGGLE:$id');
  }

  /// Set appliance dimming level (0-100).
  Future<void> setIntensity(int id, int intensity) async {
    final app = appliances[id - 1];
    app.intensity = intensity;
    app.isOn = intensity > 0;
    activeScene = null;
    notifyListeners();

    if (intensity == 0) {
      await sendCommand('OFF:$id');
    } else if (intensity == 100) {
      await sendCommand('ON:$id');
    } else {
      await sendCommand('DIM:$id,$intensity');
    }
  }

  /// Activate a scene preset.
  Future<void> activateScene(Scene scene) async {
    // Optimistic update from preset data
    for (final entry in scene.states.entries) {
      final app = appliances[entry.key - 1];
      app.intensity = entry.value;
      app.isOn = entry.value > 0;
    }
    activeScene = scene.id;
    notifyListeners();

    await sendCommand('SCENE:${scene.id}');
  }

  /// Emergency stop — all off.
  Future<void> emergencyStop() async {
    for (final app in appliances) {
      app.isOn = false;
      app.intensity = 0;
    }
    activeScene = null;
    notifyListeners();

    await sendCommand('EMERGENCY');
  }

  /// Request status refresh.
  Future<void> refreshStatus() async {
    await sendCommand('STATUS');
  }

  // ═════════════════════════════════════════════════════════════════════════
  // LOGGING
  // ═════════════════════════════════════════════════════════════════════════

  void _addLog(String message, LogType type) {
    logEntries.add(LogEntry(
      message: message,
      type: type,
      timestamp: DateTime.now(),
    ));
    if (logEntries.length > _maxLogEntries) {
      logEntries.removeAt(0);
    }
    // Don't call notifyListeners here — caller does it or it's part of a batch
  }

  void clearLog() {
    logEntries.clear();
    _addLog('Log cleared', LogType.system);
    notifyListeners();
  }

  // ═════════════════════════════════════════════════════════════════════════
  // CLEANUP
  // ═════════════════════════════════════════════════════════════════════════

  @override
  void dispose() {
    _notifySub?.cancel();
    _connectionSub?.cancel();
    _device?.disconnect();
    super.dispose();
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// LOG ENTRY MODEL
// ═══════════════════════════════════════════════════════════════════════════

enum LogType { system, sent, received, error }

class LogEntry {
  final String message;
  final LogType type;
  final DateTime timestamp;

  const LogEntry({
    required this.message,
    required this.type,
    required this.timestamp,
  });

  String get formattedTime {
    final h = timestamp.hour.toString().padLeft(2, '0');
    final m = timestamp.minute.toString().padLeft(2, '0');
    final s = timestamp.second.toString().padLeft(2, '0');
    return '$h:$m:$s';
  }
}
