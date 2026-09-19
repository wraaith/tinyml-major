import 'package:flutter/material.dart';

import '../theme/app_theme.dart';
import '../models/appliance.dart';

/// A compact scene-preset button with emoji icon.
class SceneButton extends StatelessWidget {
  final Scene scene;
  final bool isActive;
  final VoidCallback onPressed;

  const SceneButton({
    super.key,
    required this.scene,
    required this.isActive,
    required this.onPressed,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onPressed,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 250),
        curve: Curves.easeOutCubic,
        padding: const EdgeInsets.symmetric(vertical: 14),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(14),
          color: isActive
              ? AppTheme.accentBlue.withValues(alpha: 0.12)
              : AppTheme.bgGlass,
          border: Border.all(
            color: isActive
                ? AppTheme.accentBlue.withValues(alpha: 0.35)
                : AppTheme.borderGlass,
          ),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(scene.emoji, style: const TextStyle(fontSize: 22)),
            const SizedBox(height: 5),
            Text(
              scene.name,
              style: TextStyle(
                fontSize: 11,
                fontWeight: FontWeight.w600,
                color: isActive ? AppTheme.accentBlue : AppTheme.textSecondary,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
