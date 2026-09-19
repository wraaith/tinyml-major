import 'dart:math';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../theme/app_theme.dart';
import '../services/ble_service.dart';

/// Premium BLE connection screen with animated orb and scan trigger.
class ConnectionScreen extends StatefulWidget {
  const ConnectionScreen({super.key});

  @override
  State<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends State<ConnectionScreen>
    with TickerProviderStateMixin {
  late final AnimationController _orbCtrl;
  late final AnimationController _pulseCtrl;

  @override
  void initState() {
    super.initState();
    _orbCtrl = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 20),
    )..repeat();
    _pulseCtrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1800),
    )..repeat(reverse: true);
  }

  @override
  void dispose() {
    _orbCtrl.dispose();
    _pulseCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();

    return Scaffold(
      body: Container(
        decoration: BoxDecoration(
          gradient: RadialGradient(
            center: const Alignment(0, -0.3),
            radius: 1.2,
            colors: [
              AppTheme.accentBlue.withValues(alpha: 0.08),
              AppTheme.bgPrimary,
            ],
          ),
        ),
        child: SafeArea(
          child: Center(
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 36),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  // ── Animated Orb ─────────────────────────────────────
                  _buildOrb(),
                  const SizedBox(height: 32),

                  // ── Title ────────────────────────────────────────────
                  RichText(
                    text: TextSpan(
                      style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                        fontWeight: FontWeight.w800,
                        letterSpacing: -0.5,
                      ),
                      children: const [
                        TextSpan(text: 'SmartHome'),
                        TextSpan(
                          text: 'Ctrl',
                          style: TextStyle(color: AppTheme.accentCyan),
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    'BLE Appliance Controller',
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                      color: AppTheme.textSecondary,
                    ),
                  ),
                  const SizedBox(height: 48),

                  // ── Connect Button ───────────────────────────────────
                  _buildConnectButton(ble),
                  const SizedBox(height: 16),

                  // ── Status message ───────────────────────────────────
                  AnimatedSwitcher(
                    duration: const Duration(milliseconds: 250),
                    child: Text(
                      ble.statusMessage,
                      key: ValueKey(ble.statusMessage),
                      textAlign: TextAlign.center,
                      style: TextStyle(
                        fontSize: 12,
                        color: ble.statusMessage.toLowerCase().contains('error') ||
                                ble.statusMessage.toLowerCase().contains('failed')
                            ? AppTheme.accentRed
                            : AppTheme.textMuted,
                      ),
                    ),
                  ),
                  const SizedBox(height: 56),

                  // ── BLE Badge ────────────────────────────────────────
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(100),
                      color: AppTheme.bgGlass,
                      border: Border.all(color: AppTheme.borderGlass),
                    ),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(Icons.bluetooth, size: 14, color: AppTheme.textMuted),
                        const SizedBox(width: 6),
                        Text(
                          'BLUETOOTH LOW ENERGY',
                          style: TextStyle(
                            fontSize: 10,
                            fontWeight: FontWeight.w600,
                            letterSpacing: 0.8,
                            color: AppTheme.textMuted,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }

  /// Animated concentric orb with rotating rings.
  Widget _buildOrb() {
    return SizedBox(
      width: 180,
      height: 180,
      child: AnimatedBuilder(
        animation: Listenable.merge([_orbCtrl, _pulseCtrl]),
        builder: (context, _) {
          final pulse = 0.9 + 0.1 * _pulseCtrl.value;
          return Stack(
            alignment: Alignment.center,
            children: [
              // Ring 3 (outer)
              Transform.rotate(
                angle: _orbCtrl.value * 2 * pi,
                child: _ring(90, AppTheme.accentCyan.withValues(alpha: 0.07)),
              ),
              // Ring 2
              Transform.rotate(
                angle: -_orbCtrl.value * 2 * pi * 0.7,
                child: _ring(76, AppTheme.accentPurple.withValues(alpha: 0.10)),
              ),
              // Ring 1
              Transform.rotate(
                angle: _orbCtrl.value * 2 * pi * 0.5,
                child: _ring(62, AppTheme.accentBlue.withValues(alpha: 0.15)),
              ),
              // Core
              Transform.scale(
                scale: pulse,
                child: Container(
                  width: 100,
                  height: 100,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    gradient: RadialGradient(
                      center: const Alignment(-0.3, -0.3),
                      colors: [
                        AppTheme.accentBlue.withValues(alpha: 0.25),
                        AppTheme.bgSecondary,
                      ],
                    ),
                  ),
                  child: Icon(
                    Icons.wifi_tethering,
                    size: 40,
                    color: AppTheme.accentCyan,
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }

  Widget _ring(double radius, Color color) {
    return Container(
      width: radius * 2,
      height: radius * 2,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        border: Border.all(color: color, width: 1.5),
      ),
    );
  }

  /// Gradient connect button with loading state.
  Widget _buildConnectButton(BleService ble) {
    return SizedBox(
      width: double.infinity,
      height: 56,
      child: DecoratedBox(
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(100),
          gradient: const LinearGradient(
            colors: [AppTheme.accentBlue, AppTheme.accentCyan],
          ),
          boxShadow: [
            BoxShadow(
              color: AppTheme.accentBlue.withValues(alpha: 0.3),
              blurRadius: 24,
              offset: const Offset(0, 8),
            ),
          ],
        ),
        child: ElevatedButton(
          onPressed: ble.isScanning ? null : () => ble.scanAndConnect(),
          style: ElevatedButton.styleFrom(
            backgroundColor: Colors.transparent,
            shadowColor: Colors.transparent,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(100),
            ),
          ),
          child: ble.isScanning
              ? SizedBox(
                  width: 24,
                  height: 24,
                  child: CircularProgressIndicator(
                    strokeWidth: 2.5,
                    color: Colors.white,
                  ),
                )
              : Text(
                  'Connect to Device',
                  style: TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.w600,
                    color: Colors.white,
                  ),
                ),
        ),
      ),
    );
  }
}
