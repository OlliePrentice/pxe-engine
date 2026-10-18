#include "pxe/fire.hpp"

#include <algorithm>
#include <cmath>

namespace pxe {
namespace {

// Frame rates of the three parts, after the video: each steps on its own
// beat so they never all move at once.
constexpr float kCoreRate = 8.f;
constexpr float kFlameRate = 12.f;
constexpr float kEmberRate = 10.f;
constexpr float kSmokeRate = 6.f;
constexpr float kWarmUp = 3.f;  // seconds simulated before a restart's first frame

}  // namespace

Fire::Fire(glm::vec3 base, std::uint32_t seed) : base_(base), seed_(seed), rng_(seed) {}

float Fire::random() {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return static_cast<float>(rng_ & 0xffffff) / 16777216.f;
}

float Fire::signedRandom() { return random() * 2.f - 1.f; }

void Fire::restart(float time) {
  rng_ = seed_ ? seed_ : 1u;
  core_.clear();
  flames_.clear();
  embers_.clear();
  smoke_.clear();
  const float start = time - kWarmUp;
  coreTick_ = static_cast<long long>(std::floor(start * kCoreRate));
  flameTick_ = static_cast<long long>(std::floor(start * kFlameRate));
  emberTick_ = static_cast<long long>(std::floor(start * kEmberRate));
  smokeTick_ = static_cast<long long>(std::floor(start * kSmokeRate));
}

void Fire::advanceTo(float time, glm::vec2 wind) {
  if (time_ < 0.f || time < time_ || time - time_ > 2.f) restart(time);
  time_ = time;
  const auto coreTo = static_cast<long long>(std::floor(time * kCoreRate));
  const auto flameTo = static_cast<long long>(std::floor(time * kFlameRate));
  const auto emberTo = static_cast<long long>(std::floor(time * kEmberRate));
  const auto smokeTo = static_cast<long long>(std::floor(time * kSmokeRate));
  // Interleave the three so a long catch-up still mixes them in time order.
  while (coreTick_ < coreTo || flameTick_ < flameTo || emberTick_ < emberTo || smokeTick_ < smokeTo) {
    const float tc = coreTick_ < coreTo ? static_cast<float>(coreTick_ + 1) / kCoreRate : 1e30f;
    const float tf = flameTick_ < flameTo ? static_cast<float>(flameTick_ + 1) / kFlameRate : 1e30f;
    const float te = emberTick_ < emberTo ? static_cast<float>(emberTick_ + 1) / kEmberRate : 1e30f;
    const float ts = smokeTick_ < smokeTo ? static_cast<float>(smokeTick_ + 1) / kSmokeRate : 1e30f;
    if (ts <= tc && ts <= tf && ts <= te) {
      ++smokeTick_;
      stepSmoke(wind);
    } else if (tc <= tf && tc <= te) {
      ++coreTick_;
      stepCore();
    } else if (tf <= te) {
      ++flameTick_;
      stepFlames(wind);
    } else {
      ++emberTick_;
      stepEmbers(wind);
    }
  }
  collect();
  // The light breathes gently rather than jumping with the flames.
  flicker_ = 0.94f + 0.035f * std::sin(time * 7.3f) + 0.025f * std::sin(time * 12.7f + 1.3f);
}

// The core: a few large, hot blobs low in the fire, redrawn in new places
// each beat. They are what the yellow heart of the flame is made of.
void Fire::stepCore() {
  core_.clear();
  for (int i = 0; i < 3; ++i) {
    Particle p{};
    p.pos = base_ + glm::vec3{0.06f * signedRandom(), 0.06f + 0.12f * random(), 0.06f * signedRandom()};
    p.radius = 0.19f + 0.05f * random();
    p.heat = 0.8f + 0.15f * random();
    core_.push_back(p);
  }
}

// Flames: born at the base, they rise faster as they go, wander, lean with
// the wind and are pulled back toward the middle low down so the fire keeps
// a teardrop shape. They shrink a little and cool a lot as they age, so the
// top breaks up into separate red tongues that fade out whole rather than
// dwindling to specks.
void Fire::stepFlames(glm::vec2 wind) {
  const float dt = 1.f / kFlameRate;
  for (Particle& p : flames_) {
    p.age += dt;
    p.vel.y += 1.1f * dt;
    p.vel.x += 0.35f * signedRandom() + (wind.x * 0.5f - 2.2f * (p.pos.x - base_.x)) * dt;
    p.vel.z += 0.35f * signedRandom() + (wind.y * 0.5f - 2.2f * (p.pos.z - base_.z)) * dt;
    p.vel.x *= 0.8f;
    p.vel.z *= 0.8f;
    p.pos += p.vel * dt;
  }
  flames_.erase(std::remove_if(flames_.begin(), flames_.end(), [](const Particle& p) { return p.age >= p.life; }),
                flames_.end());
  for (int i = 0; i < 5; ++i) {
    Particle p{};
    p.pos = base_ + glm::vec3{0.1f * signedRandom(), 0.04f * random(), 0.1f * signedRandom()};
    p.vel = {0.1f * signedRandom(), 0.6f + 0.5f * random(), 0.1f * signedRandom()};
    p.life = 0.45f + 0.45f * random();
    p.radius = 0.15f + 0.08f * random();
    p.heat = 0.8f + 0.2f * random();
    flames_.push_back(p);
  }
}

// Embers: a few sparks a second that float up, drift on the wind and wander,
// fading from orange to dark red before they go out.
void Fire::stepEmbers(glm::vec2 wind) {
  const float dt = 1.f / kEmberRate;
  for (Particle& p : embers_) {
    p.age += dt;
    p.vel.x += 0.25f * signedRandom() + wind.x * 0.6f * dt;
    p.vel.z += 0.25f * signedRandom() + wind.y * 0.6f * dt;
    p.vel.x *= 0.9f;
    p.vel.z *= 0.9f;
    p.pos += p.vel * dt;
  }
  embers_.erase(std::remove_if(embers_.begin(), embers_.end(), [](const Particle& p) { return p.age >= p.life; }),
                embers_.end());
  if (random() < 0.35f) {
    Particle p{};
    p.pos = base_ + glm::vec3{0.15f * signedRandom(), 0.3f + 0.25f * random(), 0.15f * signedRandom()};
    p.vel = {0.15f * signedRandom(), 0.5f + 0.5f * random(), 0.15f * signedRandom()};
    p.life = 1.2f + 1.6f * random();
    embers_.push_back(p);
  }
}

// Smoke: a puff or so a beat from well above the flames, where they have
// burnt out, rising slowly, swelling,
// carried off by the wind and thinning until it is gone. It is drawn very
// faint, as a haze over what is behind it.
void Fire::stepSmoke(glm::vec2 wind) {
  const float dt = 1.f / kSmokeRate;
  for (Particle& p : smoke_) {
    p.age += dt;
    p.vel.x += 0.08f * signedRandom() + wind.x * 0.5f * dt;
    p.vel.z += 0.08f * signedRandom() + wind.y * 0.5f * dt;
    p.vel.x *= 0.92f;
    p.vel.z *= 0.92f;
    p.pos += p.vel * dt;
  }
  smoke_.erase(std::remove_if(smoke_.begin(), smoke_.end(), [](const Particle& p) { return p.age >= p.life; }),
               smoke_.end());
  if (random() < 0.8f) {
    Particle p{};
    p.pos = base_ + glm::vec3{0.08f * signedRandom(), 1.45f + 0.2f * random(), 0.08f * signedRandom()};
    p.vel = {0.05f * signedRandom(), 0.32f + 0.15f * random(), 0.05f * signedRandom()};
    p.life = 3.f + 2.f * random();
    p.radius = 0.14f + 0.06f * random();
    p.heat = 0.8f + 0.2f * random();
    smoke_.push_back(p);
  }
}

void Fire::collect() {
  drawn_.clear();
  for (const Particle& p : core_) drawn_.push_back({p.pos, p.radius, p.heat, 0.f});
  for (const Particle& p : flames_) {
    const float t = p.age / p.life;
    drawn_.push_back({p.pos, p.radius * (1.f - 0.4f * t), p.heat * (1.f - 0.75f * t), 1.f});
  }
  for (const Particle& p : embers_) drawn_.push_back({p.pos, 0.f, 1.f - p.age / p.life, 2.f});
  for (const Particle& p : smoke_) {
    const float t = p.age / p.life;
    drawn_.push_back({p.pos, p.radius * (1.f + 2.2f * t), p.heat * (1.f - t), 3.f});
  }
}

}  // namespace pxe
