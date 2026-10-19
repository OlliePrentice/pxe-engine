#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace pxe {

// One camera update. `eye` is already snapped onto the view-aligned texel
// grid. `texelError` is how far the desired eye was from that snap, in
// low-res texels, and the presenter shifts the upscale by it.
struct CameraFrame {
  glm::mat4 view{1.f};
  glm::mat4 proj{1.f};
  glm::vec3 eye{0.f};
  glm::vec3 target{0.f};
  glm::vec3 forward{0.f, 0.f, -1.f};
  glm::vec3 right{1.f, 0.f, 0.f};
  glm::vec3 up{0.f, 1.f, 0.f};
  glm::vec2 texelError{0.f};
  float texelWorld = 1.f / 16.f;
  float orthoHalfW = 1.f;
  float orthoHalfH = 1.f;
  float yawRadians = 0.f;
  float pitchRadians = 0.f;
  float nearPlane = 0.5f;
  float farPlane = 220.f;
  float pixelsPerUnit = 16.f;
};

// Orthographic camera pitched 30° down. Yaw rests on 45° steps. Translation
// is smoothed, then snapped in the camera's right/up basis so a 45° yaw
// still lands on pixel centres.
class Camera {
 public:
  Camera();

  void setTarget(glm::vec3 target);
  // Ease toward `target`, as a pan does, rather than jump there.
  void follow(glm::vec3 target) { rawPos_ = target; }
  void nudgeImmediate(glm::vec3 delta);
  void setYawRadians(float yaw);

  // Pan on the ground plane, metres per second. x is camera right, y is
  // camera forward flattened onto XZ.
  void setPanVelocity(glm::vec2 groundMetresPerSec);
  void rotateSteps(int steps);

  void update(float dt);
  CameraFrame frame(int rtW, int rtH) const;

  glm::vec3 target() const { return smoothPos_; }
  float yawRadians() const { return yaw_; }

 private:
  void basis(float yaw, glm::vec3& forward, glm::vec3& right, glm::vec3& up) const;

  glm::vec3 rawPos_{0.f};
  glm::vec3 rawVel_{0.f};
  glm::vec3 smoothPos_{0.f};
  glm::vec3 smoothVel_{0.f};
  glm::vec2 wishPan_{0.f};

  float yaw_ = 0.7853981633974483f;  // 45 degrees, the brief's view
  float yawFrom_ = yaw_;
  float yawTo_ = yaw_;
  float yawT_ = 1.f;
  bool yawAnimating_ = false;
};

}  // namespace pxe
