#pragma once

#include <glm/vec2.hpp>

#include <string>

namespace pxe {

class Window;

enum class Key {
  kA, kB, kC, kD, kE, kF, kG, kH, kI, kJ, kK, kL, kM,
  kN, kO, kP, kQ, kR, kS, kT, kU, kV, kW, kX, kY, kZ,
  k0, k1, k2, k3, k4, k5, k6, k7, k8, k9,
  kF1, kF2, kF3, kF4, kF5, kF6, kF7, kF8, kF9, kF10, kF11, kF12,
  kLeft, kRight, kUp, kDown,
  kSpace, kTab, kEscape, kEnter, kBackspace,
  kInsert, kDelete, kHome, kEnd, kPageUp, kPageDown,
  kLeftShift, kRightShift, kLeftControl, kRightControl, kLeftAlt, kRightAlt, kLeftSuper, kRightSuper,
  kLeftBracket, kRightBracket, kMinus, kEqual, kComma, kPeriod, kSlash, kBackslash, kSemicolon,
  kApostrophe, kGraveAccent,
  kCount
};

enum class MouseButton { kLeft, kRight, kMiddle, kCount };

// Named for the Xbox layout; other pads map to the same positions (kA is
// the bottom face button, kX the left one, and so on).
enum class GamepadButton {
  kA, kB, kX, kY,
  kLeftBumper, kRightBumper,
  kBack, kStart, kGuide,
  kLeftStick, kRightStick,  // the sticks pressed in
  kDpadUp, kDpadRight, kDpadDown, kDpadLeft,
  kCount
};

enum class GamepadStick { kLeft, kRight };

// Axes as the pad reports them: sticks -1..1, y pointing down (toward the
// player); triggers -1 released .. 1 fully pulled.
enum class GamepadAxis { kLeftX, kLeftY, kRightX, kRightY, kLeftTrigger, kRightTrigger, kCount };

// The keyboard, mouse and first connected gamepad, read from a window once
// a frame. "Down" is the state now; "pressed" is down now and not on the
// previous update, so a press counts once however long it is held.
class Input {
 public:
  // Read every device. Call once a frame, after Window::pollEvents.
  void update(const Window& window);

  bool keyDown(Key key) const { return keys_[index(key)]; }
  bool keyPressed(Key key) const { return keys_[index(key)] && !prevKeys_[index(key)]; }

  // The cursor in framebuffer pixels from the window's top left.
  glm::vec2 mousePosition() const { return mouse_; }
  bool mouseDown(MouseButton b) const { return buttons_[index(b)]; }
  bool mousePressed(MouseButton b) const { return buttons_[index(b)] && !prevButtons_[index(b)]; }

  // The first connected gamepad. While none is connected, every button is
  // up and every axis at rest.
  bool gamepadConnected() const { return padConnected_; }
  const std::string& gamepadName() const { return padName_; }
  bool gamepadDown(GamepadButton b) const { return pad_[index(b)]; }
  bool gamepadPressed(GamepadButton b) const { return pad_[index(b)] && !prevPad_[index(b)]; }
  // A stick after a radial dead zone (kStickDeadZone), the rest rescaled to
  // a length of 0..1, so it moves smoothly from rest to full in every
  // direction. Same axes as GamepadAxis: y points down.
  glm::vec2 gamepadStick(GamepadStick stick) const;
  // An axis as reported, without a dead zone.
  float gamepadAxis(GamepadAxis axis) const { return axes_[index(axis)]; }

  // Sticks rest a little off centre.
  static constexpr float kStickDeadZone = 0.18f;

 private:
  template <typename E>
  static int index(E e) { return static_cast<int>(e); }

  bool keys_[static_cast<int>(Key::kCount)] = {};
  bool prevKeys_[static_cast<int>(Key::kCount)] = {};
  glm::vec2 mouse_{0.f};
  bool buttons_[static_cast<int>(MouseButton::kCount)] = {};
  bool prevButtons_[static_cast<int>(MouseButton::kCount)] = {};
  bool padConnected_ = false;
  std::string padName_;
  bool pad_[static_cast<int>(GamepadButton::kCount)] = {};
  bool prevPad_[static_cast<int>(GamepadButton::kCount)] = {};
  float axes_[static_cast<int>(GamepadAxis::kCount)] = {0.f, 0.f, 0.f, 0.f, -1.f, -1.f};
  bool mappingsAdded_ = false;
};

}  // namespace pxe
