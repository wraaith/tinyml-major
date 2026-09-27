import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';

import '../theme/app_theme.dart';
import '../services/ble_service.dart';
import '../models/appliance.dart';
import '../widgets/appliance_card.dart';
import '../widgets/scene_button.dart';
import '../widgets/emergency_button.dart';
import '../widgets/ble_terminal.dart';

/// Main dashboard screen showing appliance controls, scenes, and BLE terminal.
class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    // Listen for errors and show snackbar
    final ble = context.watch<BleService>();
    if (ble.lastError != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _showErrorSnackbar(ble.lastError!);
        ble.clearError();
      });
    }
  }

  void _showErrorSnackbar(String message) {
    if (!mounted) return;
    HapticFeedback.heavyImpact();
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Row(
          children: [
            const Icon(Icons.error_outline, color: Colors.white, size: 18),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                message,
                style: const TextStyle(
                  fontWeight: FontWeight.w500,
                  fontSize: 13,
                ),
              ),
            ),
          ],
        ),
        backgroundColor: AppTheme.accentRed.withValues(alpha: 0.9),
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        margin: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        duration: const Duration(seconds: 3),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();

    return Scaffold(
      body: Container(
        decoration: BoxDecoration(
          gradient: RadialGradient(
            center: const Alignment(-0.5, -0.8),
            radius: 1.5,
            colors: [
              AppTheme.accentBlue.withValues(alpha: 0.05),
              AppTheme.bgPrimary,
            ],
          ),
        ),
        child: SafeArea(
          child: Column(
            children: [
              // ── Top Bar ─────────────────────────────────────────────
              _buildTopBar(context, ble),

              // ── Reconnecting banner ────────────────────────────────
              if (ble.isReconnecting)
                _buildReconnectBanner(),

              // ── Scrollable Content ──────────────────────────────────
              Expanded(
                child: ListView(
                  padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
                  children: [
                    // Appliance grid (2×2)
                    _buildApplianceGrid(ble),
                    const SizedBox(height: 20),

                    // Scenes
                    _sectionTitle('Scenes'),
                    const SizedBox(height: 10),
                    _buildSceneRow(ble),
                    const SizedBox(height: 20),

                    // Emergency Stop
                    EmergencyButton(onPressed: () {
                      HapticFeedback.heavyImpact();
                      ble.emergencyStop();
                    }),
                    const SizedBox(height: 20),

                    // Terminal
                    _sectionTitle('BLE Log'),
                    const SizedBox(height: 10),
                    BleTerminal(
                      logEntries: ble.logEntries,
                      onClear: () => ble.clearLog(),
                      onSend: (cmd) => ble.sendCommand(cmd),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  // ═══════════════════════════════════════════════════════════════════════
  // RECONNECTING BANNER
  // ═══════════════════════════════════════════════════════════════════════

  Widget _buildReconnectBanner() {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: [
            AppTheme.accentAmber.withValues(alpha: 0.15),
            AppTheme.accentAmber.withValues(alpha: 0.05),
          ],
        ),
        border: Border(
          bottom: BorderSide(
            color: AppTheme.accentAmber.withValues(alpha: 0.3),
          ),
        ),
      ),
      child: Row(
        children: [
          SizedBox(
            width: 14,
            height: 14,
            child: CircularProgressIndicator(
              strokeWidth: 2,
              color: AppTheme.accentAmber,
            ),
          ),
          const SizedBox(width: 10),
          Text(
            'Connection lost — reconnecting…',
            style: TextStyle(
              fontSize: 12,
              fontWeight: FontWeight.w600,
              color: AppTheme.accentAmber,
            ),
          ),
        ],
      ),
    );
  }

  // ═══════════════════════════════════════════════════════════════════════
  // TOP BAR
  // ═══════════════════════════════════════════════════════════════════════

  Widget _buildTopBar(BuildContext context, BleService ble) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      decoration: BoxDecoration(
        color: AppTheme.bgPrimary.withValues(alpha: 0.85),
        border: Border(
          bottom: BorderSide(color: AppTheme.borderGlass),
        ),
      ),
      child: Row(
        children: [
          // Connection indicator + name
          _AnimatedDot(isConnected: ble.isConnected),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  ble.deviceName,
                  style: const TextStyle(
                    fontSize: 15,
                    fontWeight: FontWeight.w700,
                    color: AppTheme.textPrimary,
                  ),
                ),
                Text(
                  ble.isConnected ? 'Connected' : 'Disconnected',
                  style: TextStyle(
                    fontSize: 10,
                    fontWeight: FontWeight.w500,
                    color: ble.isConnected
                        ? AppTheme.accentGreen
                        : AppTheme.textMuted,
                  ),
                ),
              ],
            ),
          ),

          // Refresh button
          _topBarButton(
            icon: Icons.refresh_rounded,
            tooltip: 'Refresh Status',
            onPressed: () {
              HapticFeedback.lightImpact();
              ble.refreshStatus();
            },
          ),
          const SizedBox(width: 8),

          // Disconnect button
          _topBarButton(
            icon: Icons.close_rounded,
            tooltip: 'Disconnect',
            onPressed: () {
              HapticFeedback.mediumImpact();
              ble.disconnect();
            },
            danger: true,
          ),
        ],
      ),
    );
  }

  Widget _topBarButton({
    required IconData icon,
    required String tooltip,
    required VoidCallback onPressed,
    bool danger = false,
  }) {
    return Tooltip(
      message: tooltip,
      child: Material(
        color: AppTheme.bgGlass,
        borderRadius: BorderRadius.circular(10),
        child: InkWell(
          onTap: onPressed,
          borderRadius: BorderRadius.circular(10),
          child: Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(10),
              border: Border.all(color: AppTheme.borderGlass),
            ),
            child: Icon(
              icon,
              size: 20,
              color: danger ? AppTheme.accentRed : AppTheme.textSecondary,
            ),
          ),
        ),
      ),
    );
  }

  // ═══════════════════════════════════════════════════════════════════════
  // APPLIANCE GRID
  // ═══════════════════════════════════════════════════════════════════════

  Widget _buildApplianceGrid(BleService ble) {
    return GridView.count(
      crossAxisCount: 2,
      mainAxisSpacing: 14,
      crossAxisSpacing: 14,
      childAspectRatio: 0.78,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      children: ble.appliances
          .map((app) => ApplianceCard(
                appliance: app,
                accentColor: AppTheme.applianceColor(app.id),
                onToggle: () {
                  HapticFeedback.mediumImpact();
                  ble.toggleAppliance(app.id);
                },
                onIntensityChanged: (val) => ble.setIntensity(app.id, val),
              ))
          .toList(),
    );
  }

  // ═══════════════════════════════════════════════════════════════════════
  // SCENES
  // ═══════════════════════════════════════════════════════════════════════

  Widget _buildSceneRow(BleService ble) {
    return Row(
      children: Scene.presets
          .map((scene) => Expanded(
                child: Padding(
                  padding: EdgeInsets.only(
                    left: scene.id == 1 ? 0 : 5,
                    right: scene.id == 4 ? 0 : 5,
                  ),
                  child: SceneButton(
                    scene: scene,
                    isActive: ble.activeScene == scene.id,
                    onPressed: () {
                      HapticFeedback.selectionClick();
                      ble.activateScene(scene);
                    },
                  ),
                ),
              ))
          .toList(),
    );
  }

  // ═══════════════════════════════════════════════════════════════════════
  // HELPERS
  // ═══════════════════════════════════════════════════════════════════════

  Widget _sectionTitle(String text) {
    return Text(
      text.toUpperCase(),
      style: const TextStyle(
        fontSize: 11,
        fontWeight: FontWeight.w700,
        letterSpacing: 1.2,
        color: AppTheme.textMuted,
      ),
    );
  }
}

/// Animated connection dot with a pulsing glow effect.
class _AnimatedDot extends StatefulWidget {
  final bool isConnected;
  const _AnimatedDot({required this.isConnected});

  @override
  State<_AnimatedDot> createState() => _AnimatedDotState();
}

class _AnimatedDotState extends State<_AnimatedDot>
    with SingleTickerProviderStateMixin {
  late final AnimationController _ctrl;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1500),
    )..repeat(reverse: true);
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _ctrl,
      builder: (context, _) {
        final glowRadius = widget.isConnected ? 4.0 + 6.0 * _ctrl.value : 0.0;
        return Container(
          width: 10,
          height: 10,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: widget.isConnected
                ? AppTheme.accentGreen
                : AppTheme.accentRed,
            boxShadow: widget.isConnected
                ? [
                    BoxShadow(
                      color: AppTheme.accentGreen.withValues(alpha: 0.5),
                      blurRadius: glowRadius,
                    ),
                  ]
                : null,
          ),
        );
      },
    );
  }
}
