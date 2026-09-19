import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../theme/app_theme.dart';
import '../services/ble_service.dart';
import '../models/appliance.dart';
import '../widgets/appliance_card.dart';
import '../widgets/scene_button.dart';
import '../widgets/emergency_button.dart';
import '../widgets/ble_terminal.dart';

/// Main dashboard screen showing appliance controls, scenes, and BLE terminal.
class DashboardScreen extends StatelessWidget {
  const DashboardScreen({super.key});

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
                    EmergencyButton(onPressed: () => ble.emergencyStop()),
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
          Container(
            width: 10,
            height: 10,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: ble.isConnected ? AppTheme.accentGreen : AppTheme.accentRed,
              boxShadow: ble.isConnected
                  ? [BoxShadow(color: AppTheme.accentGreen.withValues(alpha: 0.5), blurRadius: 8)]
                  : null,
            ),
          ),
          const SizedBox(width: 10),
          Text(
            ble.deviceName,
            style: TextStyle(
              fontSize: 15,
              fontWeight: FontWeight.w700,
              color: AppTheme.textPrimary,
            ),
          ),
          const Spacer(),

          // Refresh button
          _topBarButton(
            icon: Icons.refresh_rounded,
            tooltip: 'Refresh Status',
            onPressed: () => ble.refreshStatus(),
          ),
          const SizedBox(width: 8),

          // Disconnect button
          _topBarButton(
            icon: Icons.close_rounded,
            tooltip: 'Disconnect',
            onPressed: () => ble.disconnect(),
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
    return Material(
      color: AppTheme.bgGlass,
      borderRadius: BorderRadius.circular(8),
      child: InkWell(
        onTap: onPressed,
        borderRadius: BorderRadius.circular(8),
        child: Container(
          width: 38,
          height: 38,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: AppTheme.borderGlass),
          ),
          child: Icon(
            icon,
            size: 20,
            color: danger ? AppTheme.accentRed : AppTheme.textSecondary,
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
                onToggle: () => ble.toggleAppliance(app.id),
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
                    onPressed: () => ble.activateScene(scene),
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
      style: TextStyle(
        fontSize: 11,
        fontWeight: FontWeight.w700,
        letterSpacing: 1.2,
        color: AppTheme.textMuted,
      ),
    );
  }
}
