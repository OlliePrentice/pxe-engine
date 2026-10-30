#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace pxe {

// One perspective camera update, from FirstPersonCamera, for a renderer in
// RenderMode::kPerspective. The eye is not snapped to the texel grid.
// Basis as FirstPersonCamera's: forward along the view, right and up
// across it; a positive pitch looks down.
struct PerspectiveFrame {
  glm::mat4 view{1.f};
  glm::mat4 proj{1.f};
  glm::vec3 eye{0.f};
  glm::vec3 forward{0.f, 0.f, 1.f};
  glm::vec3 right{-1.f, 0.f, 0.f};
  glm::vec3 up{0.f, 1.f, 0.f};
  float yawRadians = 0.f;
  float pitchRadians = 0.f;
  float fovY = 0.f;        // vertical field of view, radians
  float nearPlane = 0.1f;  // metres
  float farPlane = 300.f;  // metres
};

// A perspective camera at an eye position, for a view from eye height: free
// yaw and pitch, a vertical field of view and a far plane. It produces a
// PerspectiveFrame for a renderer in perspective mode, drawn into the same
// low-res target as the orthographic Camera's, with a sky behind the
// geometry and haze toward the far plane. Its eye is not snapped to the
// texel grid (a perspective view can't be), so edges may crawl a little as
// it turns and moves.
//
// Yaw and pitch follow Camera's: at yaw 0 the view looks along +z, its flat
// forward is (sin yaw, 0, cos yaw) and its right (-cos yaw, 0, sin yaw), so
// a larger yaw turns left. A positive pitch looks down, as Camera's 30
// degrees does; it is held within +-89 degrees, so the view never flips.
class FirstPersonCamera {
 public:
  static constexpr float kDefaultFovY = 1.0471975511965976f;  // 60 degrees
  static constexpr float kDefaultFar = 300.f;                // metres
  static constexpr float kNear = 0.1f;                       // metres

  void setEye(glm::vec3 eye) { eye_ = eye; }
  void setYawRadians(float yaw) { yaw_ = yaw; }
  void setPitchRadians(float pitch);
  // Vertical field of view, radians, between 1 and 170 degrees.
  void setFovYRadians(float fovY);
  // How far the view reaches, metres (at least a metre past the near
  // plane). Surfaces fade into the sky's horizon colour with distance
  // (Scene::hazeDensity), fully by the far plane.
  void setFarPlane(float far);

  glm::vec3 eye() const { return eye_; }
  float yawRadians() const { return yaw_; }
  float pitchRadians() const { return pitch_; }
  float fovYRadians() const { return fovY_; }
  float farPlane() const { return far_; }

  // The view for a target of rtW x rtH (Renderer::rtW(), rtH()).
  PerspectiveFrame frame(int rtW, int rtH) const;

 private:
  glm::vec3 eye_{0.f, 1.7f, 0.f};
  float yaw_ = 0.f;
  float pitch_ = 0.f;
  float fovY_ = kDefaultFovY;
  float far_ = kDefaultFar;
};

}  // namespace pxe
