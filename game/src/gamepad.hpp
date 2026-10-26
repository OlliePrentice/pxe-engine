#pragma once

#include <glm/vec2.hpp>

namespace pxe {
class Input;
}

namespace game {

// The first connected gamepad (an Xbox controller, or any pad in its
// layout), turned into the game's actions once a frame.
//   left stick   move the hero; how far it is pushed sets the pace, walk to run
//   right stick  flick left or right to turn the view a step (like Q / E)
//   LB / RB      turn the view a step
//   X            attack (hold to charge a spin)
//   Menu         settings menu        View   pause the day
struct Gamepad {
  bool connected = false;
  glm::vec2 move{0.f};  // x screen right, y screen up, length 0..1
  int rotate = 0;       // view steps this frame
  bool toggleMenu = false;
  bool togglePause = false;
  bool attack = false;      // X pressed this frame
  bool attackHeld = false;  // and held

  // Read this frame's actions from the pad's state in `input`.
  void poll(const pxe::Input& input);

 private:
  bool flicked_ = false;  // the right stick is out and has turned the view already
};

}  // namespace game
