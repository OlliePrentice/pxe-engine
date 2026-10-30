#pragma once

#include "player.hpp"

#include "pxe/first_person_camera.hpp"
#include "pxe/lighting.hpp"
#include "pxe/scene.hpp"

#include <glm/vec2.hpp>

namespace pxe {
class Overlay;
}

namespace desert {

// The desert's clock and weather, which the settings menu edits. It starts
// paused, so play opens at the hour --hour gives.
struct Clock {
  float hour = 16.5f;           // 0-24; main sets it from --hour
  float hoursPerMinute = 6.f;   // game hours per real minute: 6 -> a day every 4 minutes
  bool paused = true;
  float cloudCover = 0.f;       // 0 clear, 1 overcast
  float cloudStrength = 0.6f;   // 0 no shadow, 1 as dark as a cast shadow
  float windDegrees = 20.f;     // direction the wind blows toward, compass-style
  float windStrength = 1.f;     // how fast it drifts the clouds
  glm::vec2 cloudOffset{0.f};

  // Advance by `dt` real seconds: the hour and the clouds' drift, both held
  // while paused.
  void advance(float dt);
  // Unit vector, world XZ, the wind blows toward.
  glm::vec2 windDir() const;
  // The weather now, for pxe::lightingAt: clouds and wind, no rain or flash.
  pxe::Weather weather() const;
};

// The player's tunables. Defaults are the game's own.
struct PlayerSettings {
  float walkSpeed = kWalkSpeed;        // metres a second, 1-10
  float sprintSpeed = kSprintSpeed;    // walkSpeed + 0.5 .. 15
  float fovDegrees = 60.f;             // vertical, 40-100
  float lookScale = 1.f;               // times main.cpp's kLookPerPoint, 0.25-4
};

// What the view draws. Defaults are what it draws without the menu.
struct ViewSettings {
  enum ArmsPreview : int { kPreviewOff = 0, kPreviewWalk = 1, kPreviewSprint = 2 };
  bool showArms = true;
  int armsPreview = kPreviewOff;   // play a clip as if walking or sprinting
  bool holdPose = false;           // with a preview: freeze it at holdPhase
  float holdPhase = 0.f;           // 0..1 through the stride
  bool edges = true;               // FrameParams::edges
  bool showNdl = false;            // FrameParams::showNdl
  bool testColours = false;        // FrameParams::testColours
  float viewDistance = pxe::FirstPersonCamera::kDefaultFar;  // metres, 100-300
};

// A settings panel toggled with Tab (or the pad's Start), built on the
// engine's overlay, drawn at window resolution over the presented view.
class SettingsMenu {
 public:
  // Draw through `overlay`, which must outlive the menu's use; applies the
  // menu's style to it.
  void init(pxe::Overlay& overlay);

  void toggle() { open_ = !open_; }
  void close() { open_ = false; }
  bool open() const { return open_; }
  // True while the menu is open and a widget takes typing.
  bool wantsKeyboard() const;

  // Build and draw the panel (if open); its Lighting section edits `scene`
  // directly, for the phase `lighting` says is in effect. Call every play frame after the
  // scene is presented, before swap.
  void draw(Clock& clock, PlayerSettings& player, ViewSettings& view, pxe::Scene& scene,
            const pxe::Lighting& lighting, float fps);

 private:
  pxe::Overlay* overlay_ = nullptr;
  bool open_ = false;
};

}  // namespace desert
