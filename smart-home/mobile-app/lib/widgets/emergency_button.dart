import 'package:flutter/material.dart';

import '../theme/app_theme.dart';

/// Full-width emergency stop button with warning styling.
class EmergencyButton extends StatelessWidget {
  final VoidCallback onPressed;

  const EmergencyButton({super.key, required this.onPressed});

  @override
  Widget build(BuildContext context) {
    return Material(
      color: AppTheme.accentRed.withValues(alpha: 0.06),
      borderRadius: BorderRadius.circular(14),
      child: InkWell(
        onTap: onPressed,
        borderRadius: BorderRadius.circular(14),
        splashColor: AppTheme.accentRed.withValues(alpha: 0.15),
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 14),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(14),
            border: Border.all(
              color: AppTheme.accentRed.withValues(alpha: 0.25),
            ),
          ),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(Icons.warning_amber_rounded,
                  size: 22, color: AppTheme.accentRed),
              const SizedBox(width: 10),
              Text(
                'EMERGENCY STOP',
                style: TextStyle(
                  fontSize: 14,
                  fontWeight: FontWeight.w700,
                  letterSpacing: 1,
                  color: AppTheme.accentRed,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
