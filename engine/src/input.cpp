#include "pxe/input.hpp"

#include "window_impl.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace pxe {
namespace {

// GLFW's key for each Key, in Key's order.
constexpr int kGlfwKeys[] = {
    GLFW_KEY_A, GLFW_KEY_B, GLFW_KEY_C, GLFW_KEY_D, GLFW_KEY_E, GLFW_KEY_F, GLFW_KEY_G, GLFW_KEY_H, GLFW_KEY_I,
    GLFW_KEY_J, GLFW_KEY_K, GLFW_KEY_L, GLFW_KEY_M, GLFW_KEY_N, GLFW_KEY_O, GLFW_KEY_P, GLFW_KEY_Q, GLFW_KEY_R,
    GLFW_KEY_S, GLFW_KEY_T, GLFW_KEY_U, GLFW_KEY_V, GLFW_KEY_W, GLFW_KEY_X, GLFW_KEY_Y, GLFW_KEY_Z,
    GLFW_KEY_0, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8,
    GLFW_KEY_9,
    GLFW_KEY_F1, GLFW_KEY_F2, GLFW_KEY_F3, GLFW_KEY_F4, GLFW_KEY_F5, GLFW_KEY_F6, GLFW_KEY_F7, GLFW_KEY_F8,
    GLFW_KEY_F9, GLFW_KEY_F10, GLFW_KEY_F11, GLFW_KEY_F12,
    GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN,
    GLFW_KEY_SPACE, GLFW_KEY_TAB, GLFW_KEY_ESCAPE, GLFW_KEY_ENTER, GLFW_KEY_BACKSPACE,
    GLFW_KEY_INSERT, GLFW_KEY_DELETE, GLFW_KEY_HOME, GLFW_KEY_END, GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN,
    GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL, GLFW_KEY_LEFT_ALT,
    GLFW_KEY_RIGHT_ALT, GLFW_KEY_LEFT_SUPER, GLFW_KEY_RIGHT_SUPER,
    GLFW_KEY_LEFT_BRACKET, GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_MINUS, GLFW_KEY_EQUAL, GLFW_KEY_COMMA,
    GLFW_KEY_PERIOD, GLFW_KEY_SLASH, GLFW_KEY_BACKSLASH, GLFW_KEY_SEMICOLON, GLFW_KEY_APOSTROPHE,
    GLFW_KEY_GRAVE_ACCENT,
};
static_assert(std::size(kGlfwKeys) == static_cast<std::size_t>(Key::kCount));

constexpr int kGlfwMouseButtons[] = {GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, GLFW_MOUSE_BUTTON_MIDDLE};
static_assert(std::size(kGlfwMouseButtons) == static_cast<std::size_t>(MouseButton::kCount));

constexpr int kGlfwPadButtons[] = {
    GLFW_GAMEPAD_BUTTON_A,           GLFW_GAMEPAD_BUTTON_B,          GLFW_GAMEPAD_BUTTON_X,
    GLFW_GAMEPAD_BUTTON_Y,           GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
    GLFW_GAMEPAD_BUTTON_BACK,        GLFW_GAMEPAD_BUTTON_START,      GLFW_GAMEPAD_BUTTON_GUIDE,
    GLFW_GAMEPAD_BUTTON_LEFT_THUMB,  GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, GLFW_GAMEPAD_BUTTON_DPAD_UP,
    GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,  GLFW_GAMEPAD_BUTTON_DPAD_DOWN,  GLFW_GAMEPAD_BUTTON_DPAD_LEFT,
};
static_assert(std::size(kGlfwPadButtons) == static_cast<std::size_t>(GamepadButton::kCount));

constexpr int kGlfwPadAxes[] = {
    GLFW_GAMEPAD_AXIS_LEFT_X,  GLFW_GAMEPAD_AXIS_LEFT_Y,        GLFW_GAMEPAD_AXIS_RIGHT_X,
    GLFW_GAMEPAD_AXIS_RIGHT_Y, GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER,
};
static_assert(std::size(kGlfwPadAxes) == static_cast<std::size_t>(GamepadAxis::kCount));

// Xbox Series controllers on newer firmware (0x0509) over Bluetooth on macOS,
// which GLFW's built-in mappings don't list yet: the same layout as the
// 0x0105 and 0x0505 firmware it does.
const char* const kExtraMappings =
    "030000005e040000130b000009050000,Xbox Series Controller,a:b0,b:b1,back:b10,dpdown:h0.4,dpleft:h0.8,"
    "dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b13,lefttrigger:a5,leftx:a0,lefty:a1,"
    "rightshoulder:b7,rightstick:b14,righttrigger:a4,rightx:a2,righty:a3,start:b11,x:b3,y:b4,platform:Mac OS X,\n";

}  // namespace

void Input::update(const Window& window) {
  GLFWwindow* win = window.impl_->window;

  std::copy(std::begin(keys_), std::end(keys_), std::begin(prevKeys_));
  for (int k = 0; k < index(Key::kCount); ++k) keys_[k] = glfwGetKey(win, kGlfwKeys[k]) == GLFW_PRESS;

  std::copy(std::begin(buttons_), std::end(buttons_), std::begin(prevButtons_));
  for (int b = 0; b < index(MouseButton::kCount); ++b)
    buttons_[b] = glfwGetMouseButton(win, kGlfwMouseButtons[b]) == GLFW_PRESS;
  // The cursor comes in window points; scale it to framebuffer pixels.
  double cx = 0.0, cy = 0.0;
  glfwGetCursorPos(win, &cx, &cy);
  int fbW = 0, fbH = 0, winW = 0, winH = 0;
  glfwGetFramebufferSize(win, &fbW, &fbH);
  glfwGetWindowSize(win, &winW, &winH);
  const float pointScale = winW > 0 ? static_cast<float>(fbW) / static_cast<float>(winW) : 1.f;
  mouse_ = glm::vec2{static_cast<float>(cx), static_cast<float>(cy)} * pointScale;

  if (!mappingsAdded_) {
    glfwUpdateGamepadMappings(kExtraMappings);
    mappingsAdded_ = true;
  }
  std::copy(std::begin(pad_), std::end(pad_), std::begin(prevPad_));
  GLFWgamepadstate state;
  int pad = -1;
  for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST && pad < 0; ++j)
    if (glfwJoystickIsGamepad(j) && glfwGetGamepadState(j, &state)) pad = j;
  const char* name = pad >= 0 ? glfwGetGamepadName(pad) : nullptr;
  if ((pad >= 0) != padConnected_)
    std::fprintf(stderr, "pxe: gamepad %s%s\n", pad >= 0 ? "connected: " : "disconnected", name ? name : "");
  padConnected_ = pad >= 0;
  if (!padConnected_) {
    padName_.clear();
    std::fill(std::begin(pad_), std::end(pad_), false);
    std::fill(std::begin(axes_), std::end(axes_), 0.f);
    axes_[index(GamepadAxis::kLeftTrigger)] = axes_[index(GamepadAxis::kRightTrigger)] = -1.f;
    return;
  }
  padName_ = name ? name : "";
  for (int b = 0; b < index(GamepadButton::kCount); ++b) pad_[b] = state.buttons[kGlfwPadButtons[b]] == GLFW_PRESS;
  for (int a = 0; a < index(GamepadAxis::kCount); ++a) axes_[a] = state.axes[kGlfwPadAxes[a]];
}

glm::vec2 Input::gamepadStick(GamepadStick stick) const {
  const bool left = stick == GamepadStick::kLeft;
  const glm::vec2 v{axes_[index(left ? GamepadAxis::kLeftX : GamepadAxis::kRightX)],
                    axes_[index(left ? GamepadAxis::kLeftY : GamepadAxis::kRightY)]};
  const float len = glm::length(v);
  if (len < kStickDeadZone) return glm::vec2{0.f};
  return v / len * std::min((len - kStickDeadZone) / (1.f - kStickDeadZone), 1.f);
}

}  // namespace pxe
