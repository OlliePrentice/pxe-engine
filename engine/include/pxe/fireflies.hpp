#pragma once

#include <glm/vec3.hpp>

#include <vector>

namespace pxe {

// One firefly at a moment: where it is and how bright, 0 dark .. 1 lit.
struct Firefly {
  glm::vec3 pos;
  float glow;
};

// Fireflies wandering round their home points and blinking. Each one's path
// and blink are fixed functions of time, so a capture at a given time always
// shows the same swarm, and nothing needs stepping from frame to frame.
std::vector<Firefly> firefliesAt(const std::vector<glm::vec3>& homes, float time);

}  // namespace pxe
