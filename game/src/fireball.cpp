#include "fireball.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float kPush = 3.f;        // m/s^2 at a full push
constexpr float kLaunchSpeed = 2.f;  // m/s
constexpr float kMaxSpeed = 3.5f;
constexpr float kRoll = 0.8f;        // rolling drag, per second
constexpr float kSmother = 3.f;      // drag once out
constexpr float kDownhill = 5.f;     // m/s^2 per unit of slope
constexpr float kStep = 0.35f;       // anything higher is a wall
constexpr float kIgniteEvery = 0.2f;  // metres rolled (or seconds sat)

}  // namespace

WorldClock::Fire* Fireball::fire(WorldClock& clock) const {
  for (WorldClock::Fire& f : clock.fires)
    if (f.id == fire_) return &f;
  return nullptr;
}

void Fireball::launch(WorldClock& clock, const pxe::Scene& scene, glm::vec2 xz, glm::vec2 heading) {
  if (alive_) clock.removeFire(fire_);
  pos_ = xz;
  vel_ = glm::length(heading) > 1e-4f ? glm::normalize(heading) * kLaunchSpeed : glm::vec2{0.f};
  const float ground = scene.ground.heightAt(xz);
  y_ = ground > pxe::HeightMap::kNone ? ground : 0.f;
  fire_ = clock.addFire(position(), scene.shelter, kSize);
  fire(clock)->level = 1.f;  // already ablaze
  alive_ = true;
  sinceIgnite_ = kIgniteEvery;
}

void Fireball::update(WorldClock& clock, const pxe::Scene& scene, float dt, glm::vec2 push, std::vector<glm::vec2>& ignite) {
  if (!alive_) return;
  WorldClock::Fire* f = fire(clock);
  if (!f) {
    alive_ = false;
    return;
  }
  // Out and done smoking: gone.
  if (!f->lit && f->level <= 0.f && f->smoke <= 0.f) {
    clock.removeFire(fire_);
    alive_ = false;
    return;
  }

  // Pushed, pulled downhill, slowed as it rolls (hard once it is out).
  const pxe::HeightMap& ground = scene.ground;
  auto height = [&](glm::vec2 p) {
    const float h = ground.heightAt(p);
    return h > pxe::HeightMap::kNone ? h : y_;
  };
  constexpr float e = 0.3f;
  const glm::vec2 slope{(height(pos_ + glm::vec2{e, 0.f}) - height(pos_ - glm::vec2{e, 0.f})) / (2.f * e),
                        (height(pos_ + glm::vec2{0.f, e}) - height(pos_ - glm::vec2{0.f, e})) / (2.f * e)};
  const float pushLen = glm::length(push);
  if (pushLen > 1.f) push /= pushLen;
  glm::vec2 accel = -glm::clamp(slope, -1.f, 1.f) * kDownhill - vel_ * (f->lit ? kRoll : kSmother);
  if (f->lit) accel += push * kPush;
  vel_ += accel * dt;
  if (glm::length(vel_) > kMaxSpeed) vel_ = glm::normalize(vel_) * kMaxSpeed;

  // Move one axis at a time, so it slides along a wall it meets at an
  // angle, and bounces off what stands more than a step above it.
  const float here = ground.cellTop(pos_);
  const glm::vec2 from = pos_;
  for (int axis = 0; axis < 2; ++axis) {
    glm::vec2 next = pos_;
    next[axis] += vel_[axis] * dt;
    const float top = ground.cellTop(next);
    if (top <= pxe::HeightMap::kNone || (here > pxe::HeightMap::kNone && top > here + kStep)) {
      vel_[axis] *= -0.4f;
      continue;
    }
    pos_ = next;
  }
  y_ = height(pos_);

  // Water puts it out at once; the rain as it does any fire open to it.
  const glm::vec3 base = position();
  f->base = base;
  f->sheltered = scene.shelter.sheltered(base);
  const float water = scene.waterTop.cellTop(pos_);
  if (water > pxe::HeightMap::kNone && water > ground.cellTop(pos_) + 0.02f) {
    f->lit = false;
    f->level = std::max(0.f, f->level - dt * 1.5f);
    f->smoke = std::max(f->smoke, 1.f - f->level);
  }

  // While it burns, it sets the grass alight along its track (and where it
  // sits).
  sinceIgnite_ += std::max(glm::length(pos_ - from), dt);
  if (f->lit && f->level > 0.3f && sinceIgnite_ >= kIgniteEvery) {
    ignite.push_back(pos_);
    sinceIgnite_ = 0.f;
  }
}

}  // namespace game
