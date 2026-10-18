#pragma once

#include <glm/vec2.hpp>

struct GLFWwindow;

namespace game {

// Game clock and weather the settings menu edits.
struct WorldClock {
  float hour = 5.5f;             // 0-24
  float hoursPerMinute = 6.f;    // game hours per real minute: 6 -> a day every 4 minutes
  bool paused = false;
  float cloudCover = 0.35f;      // 0 clear, 1 overcast
  float cloudSpeed = 1.f;        // drift multiplier
  float cloudStrength = 0.6f;    // 0 no shadow, 1 as dark as a cast shadow
  float windDegrees = 20.f;      // direction the wind blows toward, compass-style
  glm::vec2 cloudOffset{0.f};

  // Advance by `dt` real seconds.
  void advance(float dt);
  // Unit vector, world XZ, the wind blows toward.
  glm::vec2 windDir() const;
};

struct ViewToggles {
  bool grass = true;
  bool edges = true;
};

// Dear ImGui panel toggled with Tab. Drawn at window resolution on top of
// the upscaled pixel art.
class SettingsMenu {
 public:
  void init(GLFWwindow* window);
  void shutdown();

  void toggle() { open_ = !open_; }
  bool open() const { return open_; }
  // True while a text field or slider has the keyboard, so WASD etc. should
  // not move the camera.
  bool wantsKeyboard() const;

  // Build and draw the panel. Call after the scene is presented, before swap.
  void draw(WorldClock& clock, ViewToggles& view, float fps);

 private:
  bool ready_ = false;
  bool open_ = false;
};

}  // namespace game
