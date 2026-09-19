import 'package:flutter/material.dart';

import '../theme/app_theme.dart';
import '../models/appliance.dart';

/// Glassmorphism-style appliance control card with toggle + dimmer slider.
class ApplianceCard extends StatelessWidget {
  final Appliance appliance;
  final Color accentColor;
  final VoidCallback onToggle;
  final ValueChanged<int> onIntensityChanged;

  const ApplianceCard({
    super.key,
    required this.appliance,
    required this.accentColor,
    required this.onToggle,
    required this.onIntensityChanged,
  });

  /// Map icon identifiers to Material icons.
  IconData get _icon {
    switch (appliance.icon) {
      case 'light':
        return Icons.lightbulb_outline_rounded;
      case 'fan':
        return Icons.air_rounded;
      case 'heater':
        return Icons.local_fire_department_rounded;
      case 'pump':
        return Icons.water_drop_outlined;
      default:
        return Icons.power_settings_new_rounded;
    }
  }

  @override
  Widget build(BuildContext context) {
    final isOn = appliance.isOn;

    return AnimatedContainer(
      duration: const Duration(milliseconds: 350),
      curve: Curves.easeOutCubic,
      decoration: BoxDecoration(
        color: AppTheme.bgCard,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(
          color: isOn ? accentColor.withValues(alpha: 0.3) : AppTheme.borderGlass,
          width: 1,
        ),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.25),
            blurRadius: 20,
            offset: const Offset(0, 4),
          ),
          if (isOn)
            BoxShadow(
              color: accentColor.withValues(alpha: 0.18),
              blurRadius: 28,
              offset: const Offset(0, 4),
            ),
        ],
      ),
      child: Stack(
        children: [
          // Subtle inner glow when on
          if (isOn)
            Positioned.fill(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(20),
                  gradient: RadialGradient(
                    center: const Alignment(-0.5, -0.6),
                    radius: 1.3,
                    colors: [
                      accentColor.withValues(alpha: 0.06),
                      Colors.transparent,
                    ],
                  ),
                ),
              ),
            ),

          // Content
          Padding(
            padding: const EdgeInsets.all(14),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Icon
                AnimatedContainer(
                  duration: const Duration(milliseconds: 300),
                  width: 50,
                  height: 50,
                  decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(14),
                    color: isOn
                        ? accentColor.withValues(alpha: 0.12)
                        : AppTheme.bgGlass,
                    border: Border.all(
                      color: isOn
                          ? accentColor.withValues(alpha: 0.25)
                          : AppTheme.borderGlass,
                    ),
                  ),
                  child: _buildIcon(isOn),
                ),
                const SizedBox(height: 12),

                // Name + state badge
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      appliance.name,
                      style: TextStyle(
                        fontSize: 15,
                        fontWeight: FontWeight.w600,
                        color: AppTheme.textPrimary,
                      ),
                    ),
                    AnimatedContainer(
                      duration: const Duration(milliseconds: 300),
                      padding: const EdgeInsets.symmetric(
                        horizontal: 8,
                        vertical: 3,
                      ),
                      decoration: BoxDecoration(
                        borderRadius: BorderRadius.circular(100),
                        color: isOn
                            ? AppTheme.accentGreen.withValues(alpha: 0.15)
                            : AppTheme.bgGlass,
                      ),
                      child: Text(
                        isOn ? 'ON' : 'OFF',
                        style: TextStyle(
                          fontSize: 10,
                          fontWeight: FontWeight.w700,
                          letterSpacing: 1,
                          color: isOn
                              ? AppTheme.accentGreen
                              : AppTheme.textMuted,
                        ),
                      ),
                    ),
                  ],
                ),
                const Spacer(),

                // Toggle button
                Align(
                  alignment: Alignment.centerRight,
                  child: GestureDetector(
                    onTap: onToggle,
                    child: AnimatedContainer(
                      duration: const Duration(milliseconds: 300),
                      width: 44,
                      height: 44,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        color: isOn
                            ? accentColor.withValues(alpha: 0.15)
                            : AppTheme.bgGlass,
                        border: Border.all(
                          color: isOn
                              ? accentColor.withValues(alpha: 0.35)
                              : AppTheme.borderGlass,
                        ),
                      ),
                      child: Icon(
                        Icons.power_settings_new_rounded,
                        size: 22,
                        color: isOn ? accentColor : AppTheme.textMuted,
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 8),

                // Dimmer slider
                Row(
                  children: [
                    Text(
                      _dimLabel,
                      style: TextStyle(
                        fontSize: 9,
                        fontWeight: FontWeight.w600,
                        letterSpacing: 0.5,
                        color: AppTheme.textMuted,
                      ),
                    ),
                    const SizedBox(width: 6),
                    Expanded(
                      child: SliderTheme(
                        data: SliderTheme.of(context).copyWith(
                          activeTrackColor: accentColor,
                          thumbColor: accentColor,
                          overlayColor: accentColor.withValues(alpha: 0.12),
                          inactiveTrackColor: AppTheme.borderGlass,
                          trackHeight: 4,
                          thumbShape: const RoundSliderThumbShape(
                            enabledThumbRadius: 7,
                          ),
                        ),
                        child: Slider(
                          value: appliance.intensity.toDouble(),
                          min: 0,
                          max: 100,
                          divisions: 20,
                          onChanged: (v) => onIntensityChanged(v.round()),
                        ),
                      ),
                    ),
                    SizedBox(
                      width: 34,
                      child: Text(
                        '${appliance.intensity}%',
                        textAlign: TextAlign.right,
                        style: TextStyle(
                          fontSize: 11,
                          fontWeight: FontWeight.w600,
                          color: AppTheme.textSecondary,
                        ),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  /// Animated icon — fan spins when on.
  Widget _buildIcon(bool isOn) {
    if (appliance.icon == 'fan' && isOn) {
      return _SpinningIcon(icon: _icon, color: accentColor);
    }
    return Icon(_icon, size: 26, color: isOn ? accentColor : AppTheme.textSecondary);
  }

  String get _dimLabel {
    switch (appliance.icon) {
      case 'light':  return 'BRIGHTNESS';
      case 'fan':    return 'SPEED';
      case 'heater': return 'POWER';
      case 'pump':   return 'FLOW';
      default:       return 'LEVEL';
    }
  }
}

/// A continuously spinning icon widget for the fan.
class _SpinningIcon extends StatefulWidget {
  final IconData icon;
  final Color color;

  const _SpinningIcon({required this.icon, required this.color});

  @override
  State<_SpinningIcon> createState() => _SpinningIconState();
}

class _SpinningIconState extends State<_SpinningIcon>
    with SingleTickerProviderStateMixin {
  late final AnimationController _ctrl;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1200),
    )..repeat();
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return RotationTransition(
      turns: _ctrl,
      child: Icon(widget.icon, size: 26, color: widget.color),
    );
  }
}
