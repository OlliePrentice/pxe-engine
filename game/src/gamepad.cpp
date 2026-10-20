#include "gamepad.hpp"

#include <GLFW/glfw3.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdio>

namespace game {
namespace {

constexpr float kDeadZone = 0.18f;  // sticks rest a little off centre
constexpr float kFlickOut = 0.65f;  // the right stick turns the view past this
constexpr float kFlickBack = 0.3f;  // and is ready again once back inside this

// Xbox Series controllers on newer firmware (0x0509) over Bluetooth on macOS,
// which GLFW's built-in mappings don't list yet: the same layout as the
// 0x0105 and 0x0505 firmware it does.
const char* const kExtraMappings =
    "030000005e040000130b000009050000,Xbox Series Controller,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,"
    "dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:a5,leftx:a0,lefty:a1,"
    "rightshoulder:b7,rightstick:b14,righttrigger:a4,rightx:a2,righty:a3,start:b11,x:b3,y:b4,platform:Mac OS X,\n";

// A radial dead zone, the rest rescaled to 0..1, so the stick moves
// smoothly from a creep to full pace in every direction.
glm::vec2 stick(float x, float y) {
  const glm::vec2 v{x, y};
  const float len = glm::length(v);
  if (len < kDeadZone) return glm::vec2{0.f};
  return v / len * std::min((len - kDeadZone) / (1.f - kDeadZone), 1.f);
}

}  // namespace

void Gamepad::poll() {
  static const bool mapped = glfwUpdateGamepadMappings(kExtraMappings) == GLFW_TRUE;
  (void)mapped;
  move = glm::vec2{0.f};
  rotate = 0;
  toggleMenu = togglePause = attack = attackHeld = false;
  GLFWgamepadstate state;
  int pad = -1;
  for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST && pad < 0; ++j)
    if (glfwJoystickIsGamepad(j) && glfwGetGamepadState(j, &state)) pad = j;
  if ((pad >= 0) != connected)
    std::fprintf(stderr, "pxe: gamepad %s%s\n", pad >= 0 ? "connected: " : "disconnected",
                 pad >= 0 ? glfwGetGamepadName(pad) : "");
  connected = pad >= 0;
  if (!connected) {
    std::fill(std::begin(held_), std::end(held_), false);
    flicked_ = false;
    return;
  }
  auto pressed = [&](int button) {
    const bool down = state.buttons[button] == GLFW_PRESS;
    const bool edge = down && !held_[button];
    held_[button] = down;
    return edge;
  };

  // GLFW's stick y points down; up the screen is up the stick.
  move = stick(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], -state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);

  if (pressed(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER)) rotate -= 1;
  if (pressed(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER)) rotate += 1;
  const float rx = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
  if (!flicked_ && std::abs(rx) > kFlickOut) {
    rotate += rx > 0.f ? 1 : -1;
    flicked_ = true;
  } else if (flicked_ && std::abs(rx) < kFlickBack) {
    flicked_ = false;
  }
  if (pressed(GLFW_GAMEPAD_BUTTON_X)) attack = true;
  attackHeld = state.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
  if (pressed(GLFW_GAMEPAD_BUTTON_START)) toggleMenu = true;
  if (pressed(GLFW_GAMEPAD_BUTTON_BACK)) togglePause = true;
}

}  // namespace game
