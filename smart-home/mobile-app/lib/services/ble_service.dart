import 'dart:async';
import 'dart:convert';

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
  bool _reconnecting = false;
  String _deviceName = '';
  String _statusMessage = 'Tap to scan for SmartHomeCtrl';
  String? _lastError;

  bool get isConnected => _connected;
  bool get isScanning => _scanning;
  bool get isReconnecting => _reconnecting;
  String get deviceName => _deviceName;
  String get statusMessage => _statusMessage;
  String? get lastError => _lastError;

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
  Timer? _keepAliveTimer;
  int _reconnectAttempts = 0;
  static const int _maxReconnectAttempts = 3;

  // ═════════════════════════════════════════════════════════════════════════
  // SCANNING & CONNECTION
  // ═════════════════════════════════════════════════════════════════════════

  /// Clear the last error message (call from UI after showing a snackbar).
  void clearError() {
    _lastError = null;
    notifyListeners();
  }

  /// Start scanning and attempt connection to SmartHomeCtrl.
  Future<void> scanAndConnect() async {
    if (_scanning) return;

    _scanning = true;
    _lastError = null;
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
        timeout: const Duration(seconds: 15),
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
        _addLog('Scan complete — SmartHomeCtrl not found', LogType.error);
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
      await device.connect(
        timeout: const Duration(seconds: 10),
        autoConnect: false,
      );
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
      _statusMessage = 'Discovering services…';
      notifyListeners();

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
      _reconnecting = false;
      _reconnectAttempts = 0;
      _statusMessage = 'Connected';
      _addLog('Connected to $_deviceName', LogType.system);
      notifyListeners();

      // Start keepalive timer — periodically pings STATUS
      _startKeepAlive();

      // Request initial status after short delay
      await Future.delayed(const Duration(milliseconds: 600));
      await sendCommand('STATUS');
    } catch (e) {
      _statusMessage = 'Connection failed: ${e.toString()}';
      _addLog('Connection error: $e', LogType.error);
      try {
        await device.disconnect();
      } catch (_) {}
      _device = null;
      notifyListeners();
    }
  }

  /// Periodic keepalive — sends STATUS every 30s to detect stale connections.
  void _startKeepAlive() {
    _keepAliveTimer?.cancel();
    _keepAliveTimer = Timer.periodic(const Duration(seconds: 30), (_) async {
      if (_connected) {
        try {
          await sendCommand('STATUS');
        } catch (_) {
          // sendCommand will log the error
        }
      }
    });
  }

  /// Handle unexpected disconnection with auto-reconnect.
  void _onDisconnected() {
    final wasConnected = _connected;
    _connected = false;
    _rxChar = null;
    _txChar = null;
    _connectionSub?.cancel();
    _notifySub?.cancel();
    _keepAliveTimer?.cancel();

    // Reset appliance states
    for (final app in appliances) {
      app.isOn = false;
      app.intensity = 0;
    }
    activeScene = null;

    if (wasConnected && _reconnectAttempts < _maxReconnectAttempts) {
      _reconnecting = true;
      _statusMessage = 'Connection lost. Reconnecting…';
      _addLog('Disconnected — attempting reconnect (${_reconnectAttempts + 1}/$_maxReconnectAttempts)', LogType.error);
      notifyListeners();
      _attemptReconnect();
    } else {
      _reconnecting = false;
      _device = null;
      _statusMessage = 'Disconnected. Tap to reconnect.';
      _addLog('Disconnected from device', LogType.error);
      notifyListeners();
    }
  }

  /// Auto-reconnect with exponential backoff.
  Future<void> _attemptReconnect() async {
    if (_device == null) return;

    final delay = Duration(seconds: 2 * (_reconnectAttempts + 1));
    await Future.delayed(delay);

    if (!_reconnecting) return; // user manually disconnected

    _reconnectAttempts++;
    _addLog('Reconnect attempt $_reconnectAttempts/$_maxReconnectAttempts…', LogType.system);

    try {
      await _connectToDevice(_device!);
    } catch (e) {
      if (_reconnectAttempts >= _maxReconnectAttempts) {
        _reconnecting = false;
        _device = null;
        _statusMessage = 'Reconnect failed. Tap to scan again.';
        _addLog('Reconnect failed after $_maxReconnectAttempts attempts', LogType.error);
        notifyListeners();
      }
    }
  }

  /// Manually disconnect.
  Future<void> disconnect() async {
    _reconnecting = false;
    _reconnectAttempts = _maxReconnectAttempts; // prevent auto-reconnect
    _keepAliveTimer?.cancel();
    try {
      await _notifySub?.cancel();
      await _connectionSub?.cancel();
      await _device?.disconnect();
    } catch (_) {}
    _connected = false;
    _device = null;
    _rxChar = null;
    _txChar = null;

    for (final app in appliances) {
      app.isOn = false;
      app.intensity = 0;
    }
    activeScene = null;

    _statusMessage = 'Disconnected. Tap to reconnect.';
    _addLog('Manually disconnected', LogType.system);
    _reconnectAttempts = 0;
    notifyListeners();
  }

  // ═════════════════════════════════════════════════════════════════════════
  // DATA SEND / RECEIVE
  // ═════════════════════════════════════════════════════════════════════════

  /// Send a command string to the Arduino. Appends `\n` terminator.
  /// Handles chunking for BLE MTU (20 bytes).
  /// Throws on failure so callers can decide whether to update UI.
  Future<void> sendCommand(String command) async {
    if (_rxChar == null || !_connected) {
      _addLog('Cannot send — not connected', LogType.error);
      _lastError = 'Not connected to device';
      notifyListeners();
      throw Exception('Not connected');
    }

    final msg = '$command\n';
    final data = utf8.encode(msg);
    const mtu = 20;

    _addLog('→ $command', LogType.sent);

    // Determine write mode based on characteristic properties
    bool withoutResp = true;
    if (_rxChar!.properties.write) {
      withoutResp = false;
    } else if (_rxChar!.properties.writeWithoutResponse) {
      withoutResp = true;
    }

    try {
      for (int offset = 0; offset < data.length; offset += mtu) {
        final end = (offset + mtu > data.length) ? data.length : offset + mtu;
        final chunk = Uint8List.fromList(data.sublist(offset, end));
        await _rxChar!.write(chunk, withoutResponse: withoutResp);

        if (end < data.length) {
          await Future.delayed(const Duration(milliseconds: 15));
        }
      }
    } catch (e) {
      _addLog('Send error: $e', LogType.error);
      _lastError = 'Failed to send: $command';
      notifyListeners();
      rethrow;
    }
  }

  /// Handle incoming BLE notifications.
  void _onDataReceived(List<int> data) {
    final text = utf8.decode(data, allowMalformed: true);
    _rxBuffer += text;

    // Process complete lines (handle both \r\n and \n)
    while (_rxBuffer.contains('\n')) {
      final idx = _rxBuffer.indexOf('\n');
      final line = _rxBuffer.substring(0, idx).replaceAll('\r', '').trim();
      _rxBuffer = _rxBuffer.substring(idx + 1);

      if (line.isEmpty) continue;

      _addLog('← $line', LogType.received);
      _parseResponse(line);
    }

    notifyListeners();
  }

  /// Parse all response types from the firmware.
  void _parseResponse(String line) {
    // 1. Appliance status: "Appliance 1: ON (100%)"
    final statusMatch =
        RegExp(r'Appliance\s+(\d+):\s+(ON|OFF)\s+\((\d+)%\)', caseSensitive: false)
            .firstMatch(line);
    if (statusMatch != null) {
      final id = int.parse(statusMatch.group(1)!);
      final isOn = statusMatch.group(2)!.toUpperCase() == 'ON';
      final intensity = int.parse(statusMatch.group(3)!);

      if (id >= 1 && id <= 4) {
        final app = appliances[id - 1];
        app.isOn = isOn;
        app.intensity = intensity;
      }
      return;
    }

    // 2. Command acknowledgment: "OK" or "EXECUTED" or "ERR:..."
    if (line.startsWith('ERR') || line.startsWith('Error')) {
      _lastError = line;
      _addLog('⚠ $line', LogType.error);
      return;
    }

    // 3. System messages (HELP, firmware version, etc.) — just log them
  }

  // ═════════════════════════════════════════════════════════════════════════
  // HIGH-LEVEL COMMANDS
  // ═════════════════════════════════════════════════════════════════════════

  /// Toggle appliance ON/OFF. Only updates UI on success.
  Future<void> toggleAppliance(int id) async {
    try {
      await sendCommand('TOGGLE:$id');
      final app = appliances[id - 1];
      app.isOn = !app.isOn;
      app.intensity = app.isOn ? 100 : 0;
      activeScene = null;
      notifyListeners();
    } catch (e) {
      _lastError = 'Failed to toggle ${appliances[id - 1].name}';
      notifyListeners();
    }
  }

  /// Set appliance dimming level (0-100). Only updates UI on success.
  Future<void> setIntensity(int id, int intensity) async {
    try {
      if (intensity == 0) {
        await sendCommand('OFF:$id');
      } else if (intensity == 100) {
        await sendCommand('ON:$id');
      } else {
        await sendCommand('DIM:$id,$intensity');
      }
      final app = appliances[id - 1];
      app.intensity = intensity;
      app.isOn = intensity > 0;
      activeScene = null;
      notifyListeners();
    } catch (e) {
      _lastError = 'Failed to set ${appliances[id - 1].name} intensity';
      notifyListeners();
    }
  }

  /// Activate a scene preset. Only updates UI on success.
  Future<void> activateScene(Scene scene) async {
    try {
      await sendCommand('SCENE:${scene.id}');
      for (final entry in scene.states.entries) {
        final app = appliances[entry.key - 1];
        app.intensity = entry.value;
        app.isOn = entry.value > 0;
      }
      activeScene = scene.id;
      notifyListeners();
    } catch (e) {
      _lastError = 'Failed to activate ${scene.name} scene';
      notifyListeners();
    }
  }

  /// Emergency stop — all off. Only updates UI on success.
  Future<void> emergencyStop() async {
    try {
      await sendCommand('EMERGENCY');
      for (final app in appliances) {
        app.isOn = false;
        app.intensity = 0;
      }
      activeScene = null;
      notifyListeners();
    } catch (e) {
      _lastError = 'Emergency stop failed!';
      notifyListeners();
    }
  }

  /// Request status refresh.
  Future<void> refreshStatus() async {
    try {
      await sendCommand('STATUS');
    } catch (_) {}
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
    _keepAliveTimer?.cancel();
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
