import 'package:flutter/material.dart';

import '../theme/app_theme.dart';
import '../services/ble_service.dart';

/// BLE terminal log viewer with manual command input field.
class BleTerminal extends StatefulWidget {
  final List<LogEntry> logEntries;
  final VoidCallback onClear;
  final ValueChanged<String> onSend;

  const BleTerminal({
    super.key,
    required this.logEntries,
    required this.onClear,
    required this.onSend,
  });

  @override
  State<BleTerminal> createState() => _BleTerminalState();
}

class _BleTerminalState extends State<BleTerminal> {
  final _controller = TextEditingController();
  final _scrollController = ScrollController();

  @override
  void didUpdateWidget(covariant BleTerminal oldWidget) {
    super.didUpdateWidget(oldWidget);
    // Auto-scroll to bottom on new entries
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.animateTo(
          _scrollController.position.maxScrollExtent,
          duration: const Duration(milliseconds: 150),
          curve: Curves.easeOut,
        );
      }
    });
  }

  @override
  void dispose() {
    _controller.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  void _handleSend() {
    final cmd = _controller.text.trim();
    if (cmd.isNotEmpty) {
      widget.onSend(cmd);
      _controller.clear();
    }
  }

  Color _logColor(LogType type) {
    switch (type) {
      case LogType.sent:
        return AppTheme.accentCyan;
      case LogType.received:
        return AppTheme.accentGreen;
      case LogType.error:
        return AppTheme.accentRed;
      case LogType.system:
        return AppTheme.textMuted;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        // Terminal header with clear button
        Row(
          mainAxisAlignment: MainAxisAlignment.end,
          children: [
            GestureDetector(
              onTap: widget.onClear,
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(100),
                  border: Border.all(color: AppTheme.borderGlass),
                ),
                child: Text(
                  'Clear',
                  style: TextStyle(
                    fontSize: 11,
                    color: AppTheme.textMuted,
                  ),
                ),
              ),
            ),
          ],
        ),
        const SizedBox(height: 6),

        // Log viewer
        Container(
          height: 200,
          width: double.infinity,
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: Colors.black.withValues(alpha: 0.3),
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: AppTheme.borderGlass),
          ),
          child: ListView.builder(
            controller: _scrollController,
            itemCount: widget.logEntries.length,
            itemBuilder: (_, i) {
              final entry = widget.logEntries[i];
              return Padding(
                padding: const EdgeInsets.symmetric(vertical: 1),
                child: Text(
                  '[${entry.formattedTime}] ${entry.message}',
                  style: TextStyle(
                    fontFamily: 'Courier New',
                    fontSize: 11.5,
                    height: 1.6,
                    color: _logColor(entry.type),
                    fontStyle: entry.type == LogType.system
                        ? FontStyle.italic
                        : FontStyle.normal,
                  ),
                ),
              );
            },
          ),
        ),
        const SizedBox(height: 8),

        // Command input
        Row(
          children: [
            Expanded(
              child: TextField(
                controller: _controller,
                onSubmitted: (_) => _handleSend(),
                style: TextStyle(
                  fontFamily: 'Courier New',
                  fontSize: 13,
                  color: AppTheme.textPrimary,
                ),
                decoration: InputDecoration(
                  hintText: 'e.g. ON:1, STATUS, HELP',
                  hintStyle: TextStyle(
                    fontFamily: 'Courier New',
                    fontSize: 12,
                    color: AppTheme.textMuted,
                  ),
                  contentPadding: const EdgeInsets.symmetric(
                    horizontal: 14,
                    vertical: 12,
                  ),
                  filled: true,
                  fillColor: Colors.black.withValues(alpha: 0.2),
                  enabledBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(10),
                    borderSide: BorderSide(color: AppTheme.borderGlass),
                  ),
                  focusedBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(10),
                    borderSide: BorderSide(color: AppTheme.accentBlue),
                  ),
                ),
              ),
            ),
            const SizedBox(width: 8),
            Container(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(10),
                gradient: const LinearGradient(
                  colors: [AppTheme.accentBlue, AppTheme.accentCyan],
                ),
              ),
              child: Material(
                color: Colors.transparent,
                child: InkWell(
                  onTap: _handleSend,
                  borderRadius: BorderRadius.circular(10),
                  child: Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 18,
                      vertical: 12,
                    ),
                    child: Text(
                      'Send',
                      style: TextStyle(
                        fontSize: 13,
                        fontWeight: FontWeight.w600,
                        color: Colors.white,
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ],
    );
  }
}
