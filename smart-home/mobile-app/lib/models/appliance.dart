/// Data model for a single appliance on the Arduino board.
class Appliance {
  final int id;         // 1–4
  final String name;    // "Light", "Fan", etc.
  final String icon;    // Icon identifier (mapped in UI)
  bool isOn;
  int intensity;        // 0–100

  Appliance({
    required this.id,
    required this.name,
    required this.icon,
    this.isOn = false,
    this.intensity = 0,
  });

  Appliance copyWith({bool? isOn, int? intensity}) {
    return Appliance(
      id: id,
      name: name,
      icon: icon,
      isOn: isOn ?? this.isOn,
      intensity: intensity ?? this.intensity,
    );
  }
}

/// Predefined scene presets (match firmware appliance_controller.h)
class Scene {
  final int id;
  final String name;
  final String emoji;

  /// Appliance states after activation: {applianceId: intensity}
  /// intensity 0 = OFF, 1–100 = ON at that level.
  final Map<int, int> states;

  const Scene({
    required this.id,
    required this.name,
    required this.emoji,
    required this.states,
  });

  static const List<Scene> presets = [
    Scene(id: 1, name: 'Away',  emoji: '🌙', states: {1: 0, 2: 0, 3: 0, 4: 0}),
    Scene(id: 2, name: 'Home',  emoji: '🏠', states: {1: 100, 2: 100, 3: 0, 4: 0}),
    Scene(id: 3, name: 'Night', emoji: '💤', states: {1: 30, 2: 20, 3: 0, 4: 0}),
    Scene(id: 4, name: 'Party', emoji: '🎉', states: {1: 100, 2: 100, 3: 100, 4: 100}),
  ];
}
