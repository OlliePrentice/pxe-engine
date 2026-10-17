#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace pxe {

// Outputs of one camera update. The renderer draws from `eye` (already
// snapped onto the view-aligned texel grid) and the presenter shifts the
// upscale by `texelError` so the picture still glides at display resolution.
struct CameraFrame {
  glm::mat4 view{1.f};
  glm::mat4 proj{1.f};
  glm::vec3 eye{0.f};
  glm::vec3 target{0.f};
  glm::vec3 forward{0.f, 0.f, -1.f};
  glm::vec3 right{1.f, 0.f, 0.f};
  glm::vec3 up{0.f, 1.f, 0.f};
  glm::vec2 texelError{0.f};  // desired minus snapped, in low-res texels, [-0.5, 0.5]
  float texelWorld = 0.05f;
  float orthoHalfW = 1.f;
  float orthoHalfH = 1.f;
  float yawRadians = 0.f;
  float pitchRadians = 0.f;
  int pixelsPerUnit = 22;
};

// 30° pitched orthographic camera. Yaw rests on 45° steps and animates
// between them. Translation is filtered with the second-order system from
// t3ssel8r's procedural-animation notes, then snapped onto the view plane.
class Camera {
 public:
  void setTarget(glm::vec3 target);
  // Shift the look-at point with no smoothing. Used by the pixel-snap test.
  void nudgeImmediate(glm::vec3 delta);
  void setYawRadians(float yaw);
  void setPixelsPerUnitIndex(int index);

  // World-space pan on the ground plane, in units per second along
  // camera right (x) and camera forward flattened to XZ (y).
  void setPanVelocity(glm::vec2 groundUnitsPerSec);
  void rotateSteps(int steps);  // each step is 45 degrees
  void zoomSteps(int steps);

  void update(float dt);

  CameraFrame frame(int rtW, int rtH, int viewH) const;

  glm::vec3 target() const { return smoothPos_; }
  float yawRadians() const { return yaw_; }
  int pixelsPerUnit() const;
  int zoomIndex() const { return zoom_; }

 private:
  void recomputeBasis(glm::vec3& forward, glm::vec3& right, glm::vec3& up) const;

  glm::vec3 rawPos_{18.f, 0.f, 16.f};
  glm::vec3 rawVel_{0.f};
  glm::vec3 smoothPos_{18.f, 0.f, 16.f};
  glm::vec3 smoothVel_{0.f};
  glm::vec2 wishPan_{0.f};

  float yaw_ = 0.78539816339f;  // 45 degrees
  float yawFrom_ = yaw_;
  float yawTo_ = yaw_;
  float yawT_ = 1.f;
  bool yawAnimating_ = false;

  int zoom_ = 4;  // index into the pixels-per-unit table (42 px/unit)
};

}  // namespace pxe
