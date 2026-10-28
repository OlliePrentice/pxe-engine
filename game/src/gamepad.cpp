#include "gamepad.hpp"

#include "pxe/input.hpp"

#include <cmath>

namespace game {
namespace {

constexpr float kFlickOut = 0.65f;  // the right stick turns the view past this
constexpr float kFlickBack = 0.3f;  // and is ready again once back inside this

}  // namespace

void Gamepad::poll(const pxe::Input& input) {
  using Button = pxe::GamepadButton;
  move = glm::vec2{0.f};
  rotate = 0;
  toggleMenu = togglePause = attack = attackHeld = false;
  connected = input.gamepadConnected();
  if (!connected) {
    flicked_ = false;
    return;
  }

  // The pad's stick y points down; up the screen is up the stick.
  const glm::vec2 left = input.gamepadStick(pxe::GamepadStick::kLeft);
  move = {left.x, -left.y};

  if (input.gamepadPressed(Button::kLeftBumper)) rotate -= 1;
  if (input.gamepadPressed(Button::kRightBumper)) rotate += 1;
  // The flick reads the stick as reported, without the dead zone.
  const float rx = input.gamepadAxis(pxe::GamepadAxis::kRightX);
  if (!flicked_ && std::abs(rx) > kFlickOut) {
    rotate += rx > 0.f ? 1 : -1;
    flicked_ = true;
  } else if (flicked_ && std::abs(rx) < kFlickBack) {
    flicked_ = false;
  }
  if (input.gamepadPressed(Button::kX)) attack = true;
  attackHeld = input.gamepadDown(Button::kX);
  if (input.gamepadPressed(Button::kStart)) toggleMenu = true;
  if (input.gamepadPressed(Button::kBack)) togglePause = true;
}

}  // namespace game
