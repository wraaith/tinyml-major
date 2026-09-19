import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

/// Centralized premium dark-mode theme for the Smart Home app.
class AppTheme {
  AppTheme._();

  // ── Palette ──────────────────────────────────────────────────────────────
  static const Color bgPrimary   = Color(0xFF0A0E1A);
  static const Color bgSecondary = Color(0xFF111827);
  static const Color bgCard      = Color(0xFF151D2E);
  static const Color bgGlass     = Color(0x0AFFFFFF); // 4%
  static const Color borderGlass = Color(0x14FFFFFF); // 8%

  static const Color textPrimary   = Color(0xFFF0F2F5);
  static const Color textSecondary = Color(0xFF8B95A8);
  static const Color textMuted     = Color(0xFF4B5563);

  // Accent colours
  static const Color accentBlue   = Color(0xFF3B82F6);
  static const Color accentCyan   = Color(0xFF06B6D4);
  static const Color accentPurple = Color(0xFF8B5CF6);
  static const Color accentAmber  = Color(0xFFF59E0B);
  static const Color accentRed    = Color(0xFFEF4444);
  static const Color accentGreen  = Color(0xFF22C55E);

  // Per-appliance accent
  static const List<Color> applianceColors = [
    accentAmber,  // 1 — Light
    accentCyan,   // 2 — Fan
    accentRed,    // 3 — Heater
    accentGreen,  // 4 — Pump
  ];

  static Color applianceColor(int id) =>
      applianceColors[(id - 1).clamp(0, 3)];

  // ── Theme Data ───────────────────────────────────────────────────────────
  static final ThemeData darkTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    scaffoldBackgroundColor: bgPrimary,
    colorScheme: ColorScheme.dark(
      primary: accentBlue,
      secondary: accentCyan,
      surface: bgSecondary,
      error: accentRed,
    ),
    textTheme: GoogleFonts.interTextTheme(ThemeData.dark().textTheme).apply(
      bodyColor: textPrimary,
      displayColor: textPrimary,
    ),
    appBarTheme: AppBarTheme(
      backgroundColor: bgPrimary.withValues(alpha: 0.85),
      elevation: 0,
      centerTitle: false,
      titleTextStyle: GoogleFonts.inter(
        fontSize: 18,
        fontWeight: FontWeight.w700,
        color: textPrimary,
      ),
    ),
    cardTheme: CardThemeData(
      color: bgCard,
      elevation: 0,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(20),
        side: BorderSide(color: borderGlass),
      ),
    ),
    sliderTheme: SliderThemeData(
      trackHeight: 5,
      activeTrackColor: accentBlue,
      inactiveTrackColor: borderGlass,
      thumbColor: accentBlue,
      overlayColor: accentBlue.withValues(alpha: 0.12),
      thumbShape: const RoundSliderThumbShape(enabledThumbRadius: 8),
    ),
  );
}
