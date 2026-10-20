#pragma once

#include <glm/vec2.hpp>

namespace game {

// The first connected gamepad (an Xbox controller, or anything GLFW maps
// to its layout), read once a frame.
//   left stick   move the hero; how far it is pushed sets the pace, walk to run
//   right stick  flick left or right to turn the view a step (like Q / E)
//   LB / RB      turn the view a step
//   Menu         settings menu        View   pause the day
struct Gamepad {
  bool connected = false;
  glm::vec2 move{0.f};  // x screen right, y screen up, length 0..1
  int rotate = 0;       // view steps this frame
  bool toggleMenu = false;
  bool togglePause = false;

  // Poll the pad, turning presses into this frame's edges.
  void poll();

 private:
  bool held_[15] = {};
  bool flicked_ = false;  // the right stick is out and has turned the view already
};

}  // namespace game
