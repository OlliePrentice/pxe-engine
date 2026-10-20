#pragma once

#include "pxe/mesh.hpp"
#include "pxe/scene.hpp"
#include "pxe/skinned.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <vector>

namespace game {

// Second-order dynamics, after t3ssel8r's "Giving Personality to Procedural
// Animations using Math": y follows a target x like a damped spring,
//   y + k1 y' + k2 y'' = x + k3 x',
// set by its natural frequency f (Hz), damping z (0 bouncy .. 1 no
// overshoot) and initial response r (0 eases in, 1 follows at once, above 1
// overshoots, below 0 anticipates). Stepped semi-implicitly, with k2 held up
// enough to stay stable at any frame time.
class SecondOrder {
 public:
  SecondOrder(float f = 1.f, float z = 1.f, float r = 0.f, float x0 = 0.f) { set(f, z, r, x0); }
  void set(float f, float z, float r, float x0) {
    const float pi = 3.14159265f;
    k1_ = z / (pi * f);
    k2_ = 1.f / ((2.f * pi * f) * (2.f * pi * f));
    k3_ = r * z / (2.f * pi * f);
    reset(x0);
  }
  void reset(float x0) {
    xp_ = y_ = x0;
    yd_ = 0.f;
  }
  float update(float dt, float x) {
    if (dt <= 0.f) return y_;
    const float xd = (x - xp_) / dt;
    xp_ = x;
    const float k2 = std::max({k2_, dt * dt / 2.f + dt * k1_ / 2.f, dt * k1_});
    y_ += dt * yd_;
    yd_ += dt * (x + k3_ * xd - y_ - k1_ * yd_) / k2;
    return y_;
  }
  float value() const { return y_; }

 private:
  float k1_ = 0.f, k2_ = 0.f, k3_ = 0.f;
  float xp_ = 0.f, y_ = 0.f, yd_ = 0.f;
};

// What the player asks for this frame: a direction on the ground (world xz,
// length 0..1, already turned to the camera) and whether to walk rather
// than run.
struct HeroInput {
  glm::vec2 move{0.f};
  bool walk = false;
};

// The hooded hero, built, rigged and animated in Blender
// (tools/blender/build_hooded_hero.py) and loaded from
// assets/characters/hooded_hero.glb.
//
// Everything about its motion is continuous: it speeds up, slows down and
// turns smoothly; its idle, walk and run clips are sampled between keys and
// blended by speed, the walk and run kept in step on one shared phase that
// advances with the ground covered, so the feet don't slide. Over the clips
// it leans into acceleration and banks into turns, and its cape and hood
// follow through with second-order dynamics.
class Hero {
 public:
  // Load the model (needs the GL context); throws if it is missing.
  void build();
  bool built() const { return built_; }
  // Stand at xz on the ground, facing `yaw` (radians; 0 faces +z).
  void spawn(const pxe::Scene& scene, glm::vec2 xz, float yaw);
  // How big against a real person (1). Stride and speed scale with it.
  void setSize(float size) { size_ = std::clamp(size, 0.5f, 3.f); }
  float size() const { return size_; }
  void update(const pxe::Scene& scene, float dt, const HeroInput& input);
  // Skin this frame's pose and hand the figure to the frame.
  void pose(std::vector<const pxe::Mesh*>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]);

  glm::vec3 position() const { return {pos_.x, y_, pos_.y}; }
  float speed() const { return glm::length(vel_); }

 private:
  pxe::SkinnedModel model_;
  pxe::Mesh mesh_;
  pxe::SkinnedModel::Pose pose_, run_, idle_;
  std::vector<pxe::Vertex> verts_;
  bool built_ = false;
  int idleClip_ = -1, walkClip_ = -1, runClip_ = -1;
  int spine_ = -1, chest_ = -1, hips_ = -1, hood_ = -1, head_ = -1;
  struct Chain {
    int joints[3] = {-1, -1, -1};
    glm::vec3 out{0.f};  // the axis that swings it out from the body
  };
  std::vector<Chain> cape_;

  glm::vec2 pos_{0.f};
  float y_ = 0.f;
  glm::vec2 vel_{0.f};
  float yaw_ = 0.f;
  float yawRate_ = 0.f;  // radians a second, smoothed
  float phase_ = 0.f;    // 0..1 through two steps
  float clock_ = 0.f;
  float size_ = 1.6f;
  float runBlend_ = 0.f;  // 0 walk .. 1 run, smoothed
  SecondOrder turn_{2.2f, 0.85f, 0.f, 0.f};       // facing, unwrapped
  SecondOrder lagSpeed_{1.6f, 0.45f, 0.f, 0.f};   // the cape's sense of speed
  SecondOrder lagYaw_{1.4f, 0.4f, 0.f, 0.f};      // and of facing
  SecondOrder lean_{2.5f, 0.7f, 0.f, 0.f};        // forward lean from acceleration
  SecondOrder hoodLag_{2.f, 0.35f, 0.f, 0.f};
  float lastSpeed_ = 0.f;
  float fall_ = 0.f;  // metres a second, downward, off a drop
};

}  // namespace game
