#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "audio.hpp"
#include "pxe/scene.hpp"

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace game {

// Game clock and weather the settings menu edits.
struct WorldClock {
  float hour = 5.5f;             // 0-24
  float hoursPerMinute = 6.f;    // game hours per real minute: 6 -> a day every 4 minutes
  bool paused = false;
  float cloudCover = 0.35f;      // 0 clear, 1 overcast
  float cloudSpeed = 1.f;        // wind strength: cloud drift and rain lean
  float cloudStrength = 0.6f;    // 0 no shadow, 1 as dark as a cast shadow
  float windDegrees = 20.f;      // direction the wind blows toward, compass-style
  float rain = 0.f;              // 0 dry, 0.2 drizzle, 0.55 rain, 1 downpour
  bool lightning = true;         // flashes now and then in rain and downpour
  // Grass fire: seconds a fire set alight keeps spreading before it dies
  // out (pxe::FrameParams::grassFireLife); 0 until nothing is left to burn.
  float grassFireLife = 30.f;
  // The fires: the scene's to begin with (see placeFires), and any the game
  // adds as it runs. Each is lit until Rain or Downpour puts it out - unless
  // a structure shelters it - then out until relit from the menu. Its level
  // eases between out (0) and burning (1), and once out it smokes on for a
  // while (smoke 1 just out .. 0). `size` is 1 for a campfire and less
  // for a small flame - about 0.2 for a candle - which catches and goes out
  // faster.
  struct Fire {
    std::uint32_t id = 0;  // stays the fire's while it lives
    glm::vec3 base{0.f};
    bool sheltered = false;
    bool lit = true;
    float level = 1.f;
    float smoke = 0.f;
    float size = 1.f;
    bool doused = false;  // put out with water: it dies in moments, not seconds
  };
  std::vector<Fire> fires;
  std::uint32_t nextFireId = 0;
  glm::vec2 cloudOffset{0.f};

  // The flash now lighting the scene, 0 none .. 1 at its peak, and the
  // direction toward where it lights from.
  float flash = 0.f;
  glm::vec3 flashDir{0.f, 1.f, 0.f};
  int strikes = 0;  // counts flashes, so the game can follow each with thunder

  // One burning fire per scene fire, sheltered as the scene says.
  void placeFires(const std::vector<pxe::FireSite>& sites);
  // Light a new fire at `base` (sheltered from `shelter`); returns its id.
  std::uint32_t addFire(glm::vec3 base, const pxe::ShelterMap& shelter, float size = 1.f);
  void removeFire(std::uint32_t id);
  // Relight every fire that is out.
  void relightFires();
  // Water over the stroke from `from` to `to` (world xz), `radius` metres
  // wide: puts out every fire under it at once, roofed or not.
  void douse(glm::vec2 from, glm::vec2 to, float radius);
  // Advance by `dt` real seconds.
  void advance(float dt);
  // A lightning flash now, lighting the scene from a random direction.
  void strike();
  // Rain at or above this puts open campfires out (and stops them relighting).
  static constexpr float kRainPutsOutFire = 0.45f;
  // Unit vector, world XZ, the wind blows toward.
  glm::vec2 windDir() const;

 private:
  float random();
  float nextStrike_ = 3.f;    // seconds until the next flash
  float strikeAge_ = -1.f;    // seconds since the flash began; < 0 none
  bool flicker_ = false;      // the flash dips and comes back once
  unsigned rng_ = 0x9e3779b9u;
};

struct ViewToggles {
  bool grass = true;
  bool edges = true;
  bool fire = true;
  bool rays = true;
  bool fireflies = false;
};

// Spell brushes, painted into the grass fire with the left mouse button:
// fire sets it alight, water soaks it and puts fire out, regrowth brings
// burnt grass back. Kinds are pxe::FireGrid::Stroke's.
struct BrushSettings {
  static constexpr int kNone = -1;
  int kind = kNone;
  float radius = 1.f;  // metres
};

// Dear ImGui panel toggled with Tab. Drawn at window resolution on top of
// the upscaled pixel art.
class SettingsMenu {
 public:
  void init(GLFWwindow* window);
  void shutdown();

  void toggle() { open_ = !open_; }
  bool open() const { return open_; }
  // True while a text field or slider has the keyboard, so WASD etc. should
  // not move the camera.
  bool wantsKeyboard() const;
  // True while the pointer is over the panel, so a click is not a brush stroke.
  bool wantsMouse() const;

  // Build and draw the panel, and round the cursor the brush's outline on
  // the ground (window points) if any. Call after the scene is presented,
  // before swap.
  void draw(WorldClock& clock, ViewToggles& view, SoundSettings& sound, BrushSettings& brush,
            const std::vector<glm::vec2>& brushOutline, float fps);

 private:
  bool ready_ = false;
  bool open_ = false;
};

}  // namespace game
