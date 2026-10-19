#include "pxe/fire.hpp"

#include <glm/geometric.hpp>

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

int Fire::count(float n) {
  const float whole = std::floor(n);
  return static_cast<int>(whole) + (random() < n - whole ? 1 : 0);
}

void Fire::advanceTo(float time, glm::vec2 wind, float level, float smoulder) {
  level_ = std::clamp(level, 0.f, 1.f);
  smoulder_ = std::clamp(smoulder, 0.f, 1.f);
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
  const int n = count(3.f * level_);
  for (int i = 0; i < n; ++i) {
    Particle p{};
    p.pos = base_ + glm::vec3{0.06f * signedRandom(), 0.06f + 0.12f * random(), 0.06f * signedRandom()};
    p.radius = (0.19f + 0.05f * random()) * (0.45f + 0.55f * level_);
    p.heat = (0.8f + 0.15f * random()) * std::sqrt(level_);
    core_.push_back(p);
  }
}

// Coals: while the fire is dying, a few red pixels glow in the logs after
// the flames have shrunk away, going dark as the last of it goes out.
void Fire::collectCoals() {
  if (level_ <= 0.f || level_ >= 0.5f) return;
  std::uint32_t h = seed_ * 2654435761u + static_cast<std::uint32_t>(coreTick_) * 40503u;
  auto next = [&h]() {
    h ^= h << 13;
    h ^= h >> 17;
    h ^= h << 5;
    return static_cast<float>(h & 0xffffff) / 16777216.f;
  };
  for (int i = 0; i < 4; ++i) {
    const glm::vec3 at = base_ + glm::vec3{0.14f * (next() * 2.f - 1.f), 0.03f + 0.05f * next(), 0.14f * (next() * 2.f - 1.f)};
    drawn_.push_back({at, 0.f, std::min(1.f, 2.f * level_) * (0.5f + 0.5f * next()), 2.f});
  }
}

// Flames: born at the base, they rise faster as they go, wander, lean with
// the wind and are pulled back toward the middle low down so the fire keeps
// a teardrop shape. They shrink a little and cool a lot as they age, so the
// top breaks up into separate red tongues that fade out whole rather than
// dwindling to specks.
void Fire::stepFlames(glm::vec2 wind) {
  const float dt = 1.f / kFlameRate;
  // Low down the flames are drawn back to the middle, which keeps the
  // teardrop; a wind stronger than the default slackens that, so the fire
  // lays over and streams downwind instead of standing up against it.
  const float pull = 2.2f / (1.f + 0.6f * std::max(glm::length(wind) - 0.6f, 0.f));
  for (Particle& p : flames_) {
    p.age += dt;
    p.vel.y += 1.1f * dt;
    p.vel.x += 0.35f * signedRandom() + (wind.x * 0.5f - pull * (p.pos.x - base_.x)) * dt;
    p.vel.z += 0.35f * signedRandom() + (wind.y * 0.5f - pull * (p.pos.z - base_.z)) * dt;
    p.vel.x *= 0.8f;
    p.vel.z *= 0.8f;
    p.pos += p.vel * dt;
  }
  flames_.erase(std::remove_if(flames_.begin(), flames_.end(), [](const Particle& p) { return p.age >= p.life; }),
                flames_.end());
  // A dying fire puts up fewer, smaller, shorter-lived and cooler flames.
  const int n = count(5.f * level_);
  for (int i = 0; i < n; ++i) {
    Particle p{};
    p.pos = base_ + glm::vec3{0.1f * signedRandom(), 0.04f * random(), 0.1f * signedRandom()};
    p.vel = {0.1f * signedRandom(), (0.6f + 0.5f * random()) * (0.6f + 0.4f * level_), 0.1f * signedRandom()};
    p.life = (0.45f + 0.45f * random()) * (0.5f + 0.5f * level_);
    p.radius = (0.15f + 0.08f * random()) * (0.5f + 0.5f * level_);
    p.heat = (0.8f + 0.2f * random()) * (0.65f + 0.35f * level_);
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
  if (random() < 0.35f * level_ * std::min(size_, 1.f)) {
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
  // While it burns, smoke starts well above the flames; once they are gone
  // it rises straight from the wet logs, and thins as the fire smoulders out.
  const float smoke = std::max(level_, smoulder_);
  if (random() < 0.8f * smoke) {
    Particle p{};
    const float from = 0.35f + 1.1f * level_;
    p.pos = base_ + glm::vec3{0.08f * signedRandom(), from + 0.2f * random(), 0.08f * signedRandom()};
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
  collectCoals();
  for (const Particle& p : smoke_) {
    const float t = p.age / p.life;
    drawn_.push_back({p.pos, p.radius * (1.f + 2.2f * t), p.heat * (1.f - t), 3.f});
  }
  if (size_ != 1.f)
    for (FireParticle& p : drawn_) {
      p.pos = base_ + (p.pos - base_) * size_;
      p.radius *= size_;
    }
}

}  // namespace pxe
