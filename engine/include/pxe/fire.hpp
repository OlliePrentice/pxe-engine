#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace pxe {

// One particle as drawn: world position, radius in metres, heat (or smoke
// density) and kind (0 core, 1 flame, 2 ember, 3 smoke).
struct FireParticle {
  glm::vec3 pos;
  float radius;
  float heat;
  float kind;
};

static_assert(sizeof(FireParticle) == sizeof(float) * 6, "FireParticle is tightly packed");

// A campfire, after t3ssel8r's procedural pixel art fire: a plain particle
// system (about a hundred particles) drawn as soft discs into a heat buffer,
// which the post pass cuts into a few flat flame colours. The three parts of
// it - a core that sits at the base, flames that rise, shrink and cool, and
// embers that drift up on the wind - each step at their own low frame rate,
// so it moves like hand animation rather than smooth simulation. A slower
// fourth layer of faint smoke rises from the flame tips.
class Fire {
 public:
  Fire(glm::vec3 base, std::uint32_t seed);

  // Bring the fire to `time` seconds, with `wind` the horizontal wind in
  // metres per second. A first call, or a jump back or far ahead, restarts
  // it a few seconds earlier, so it is always already burning and a capture
  // at a given time always shows the same flames. `level` is how well it is
  // burning, 0 out .. 1 full; a dying fire has fewer, smaller, cooler flames.
  // `smoulder` (0..1) keeps smoke rising from the logs after it is out.
  void advanceTo(float time, glm::vec2 wind, float level = 1.f, float smoulder = 0.f);

  const std::vector<FireParticle>& particles() const { return drawn_; }
  glm::vec3 base() const { return base_; }
  // About 0.88..1, a gentle smooth pulse: scales the firelight.
  float flicker() const { return flicker_; }

 private:
  struct Particle {
    glm::vec3 pos;
    glm::vec3 vel;
    float age;
    float life;
    float radius;
    float heat;
  };

  void restart(float time);
  void stepCore();
  void stepFlames(glm::vec2 wind);
  void stepEmbers(glm::vec2 wind);
  void stepSmoke(glm::vec2 wind);
  void collect();
  void collectCoals();
  int count(float n);      // n rounded up or down at random, keeping the mean
  float random();          // 0..1
  float signedRandom();    // -1..1

  glm::vec3 base_;
  std::uint32_t seed_;
  std::uint32_t rng_;
  float time_ = -1.f;
  long long coreTick_ = 0, flameTick_ = 0, emberTick_ = 0, smokeTick_ = 0;
  std::vector<Particle> core_, flames_, embers_, smoke_;
  std::vector<FireParticle> drawn_;
  float flicker_ = 1.f;
  float level_ = 1.f;
  float smoulder_ = 0.f;
};

}  // namespace pxe
