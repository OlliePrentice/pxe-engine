#pragma once

#include <memory>

namespace game {

// Volumes the settings menu edits, 0..1 each.
struct SoundSettings {
  bool on = true;
  float master = 0.8f;
  float wind = 0.7f;
  float rain = 0.8f;
  float thunder = 1.f;
};

// Weather sound, synthesised as it plays rather than read from recordings,
// so each layer follows its setting continuously: wind that rises and falls
// with the gusts and rustles the leaves, rain whose hiss and patter thicken
// with its severity, and thunder after each lightning flash, later and
// lower the farther off it is.
class Audio {
 public:
  static constexpr int kRate = 48000;

  Audio();
  ~Audio();
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  // Opens the default output. False (and silent) if there is none.
  bool start();

  // Wind strength as the menu has it (1 default, up to 6) and rain 0..1.
  void setWeather(float windStrength, float rain);
  void setVolumes(const SoundSettings& sound);
  // Thunder for a flash: `pan` -1 left .. 1 right. Its distance, and so its
  // delay, loudness and crack, is picked at random.
  void thunder(float pan);
  // Thunder at a given distance, 0 overhead .. 1 far off.
  void thunderAt(float pan, float distance);

  // Render `frames` of interleaved stereo without a device, for --audio-wav.
  void render(float* out, int frames);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace game
