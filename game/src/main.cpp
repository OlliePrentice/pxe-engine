#include "audio.hpp"
#include "fireball.hpp"
#include "fox.hpp"
#include "gamepad.hpp"
#include "hero.hpp"
#include "scene.hpp"
#include "settings.hpp"
#include "tavern.hpp"

#include "pxe/audio_file.hpp"
#include "pxe/camera.hpp"
#include "pxe/config.hpp"
#include "pxe/image.hpp"
#include "pxe/input.hpp"
#include "pxe/lighting.hpp"
#include "pxe/overlay.hpp"
#include "pxe/renderer.hpp"
#include "pxe/scatter.hpp"
#include "pxe/window.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct Options {
  int width = 1920;
  int height = 1080;
  std::string capture;
  std::string windowPng;
  bool selfTest = false;
  bool vsync = true;
  bool hideGrass = false;
  bool magenta = false;
  bool ndl = false;
  bool testColours = false;
  bool noDepth = false;
  bool stamps = false;
  bool tufts = true;
  bool edges = true;
  bool fire = true;
  float fireLevel = 1.f;  // captures: how well every campfire burns, and its smoke once out
  float fireSmoke = 0.f;
  bool shelterTest = false;
  int extraFires = 0;
  bool candles = false;
  std::vector<glm::vec2> grassFires;  // world xz to set the lawn alight at the start
  // A fireball rolled from x,z (y), pushed toward compass heading z degrees,
  // turning w degrees a second.
  bool haveFireball = false;
  glm::vec4 fireball{0.f};
  // Spell brush strokes (see BrushSettings), each moved at an even pace
  // from `from` to `to` (world xz) between `start` and `end` seconds in.
  struct ScriptedStroke {
    int kind = 0;
    glm::vec2 from{0.f}, to{0.f};
    float start = 0.f, end = 0.f;
    float radius = 1.f;
  };
  std::vector<ScriptedStroke> brushes;
  float rainFrom = -1.f;  // captures with --settle: dry until this many seconds in, then --rain
  float fireLife = 30.f;  // seconds a grass fire spreads; 0 until all is burnt
  float settle = 0.f;  // captures: seconds of weather run first, the hour held
  bool rays = true;
  bool fireflies = false;
  float hour = -1.f;  // < 0: 15:00 for captures, 05:30 interactive
  float clouds = 0.35f;
  float rain = 0.f;
  bool lightning = true;
  bool sound = true;
  std::string audioWav;
  float flash = 0.f;  // > 0: captures hold a flash at this level
  float flashAz = 0.f, flashEl = 45.f;
  float windDegrees = 20.f;
  float windStrength = 1.f;
  bool menu = false;
  std::string uiShot;
  int bench = 0;
  bool benchPaused = false;
  float time = 0.f;
  // "ruins", "hills" or "tavern" (inside the Sleeping Fox).
  std::string scene = "ruins";
  // Every tavern door shown fully open, whoever is near.
  bool tavernDoorOpen = false;
  bool haveYaw = false;
  float yawDeg = 45.f;
  float slide = 0.f;
  float minDist = pxe::GrassScatter{}.clumpSpacing;  // metres, as scattered
  bool haveTarget = false;
  glm::vec3 target{0.f};
  // The hooded hero: on when playing, off for captures unless asked for.
  // -1 default, 0 --no-hero, 1 --hero.
  int hero = -1;
  float heroSize = 1.6f;
  // Captures: move the hero first, toward compass bearing x for y seconds
  // (walking if z is 1), with the view following.
  bool haveHeroMove = false;
  glm::vec3 heroMove{0.f};
  float heroAttack = -1.f;  // captures: this many seconds into an attack (after any move)
  int heroPresses = 1;      // ...pressing X this many times, a fifth of a second apart
  float heroHold = 0.f;     // ...holding the first press this many seconds (a charge)
  // The fox in the ruins: the hand-placed one, by the campfire, when
  // playing; in captures only when asked for; never in the hills.
  // -1 default, 0 --no-fox, 1 --fox.
  int fox = -1;
  // --foxes N: N foxes placed where they fit, in place of the hand-placed
  // one, in play, captures and the bench. -1: not asked for.
  int foxes = -1;
  // Every fox shown this many seconds into its wake sequence (0 starting to
  // stand), in captures and at the start of play; implies --fox. -1: asleep.
  float foxWake = -1.f;
  // Every fox's next wake (--fox-wake's, or in play the first) takes a walk;
  // implies --fox.
  bool foxWalk = false;
  // Foxes added as if clicked at world xz with the fox tool: in play `secs`
  // seconds in; in captures before the shot, or with --fox-wake `secs` into
  // the foxes' run up to it.
  struct AddedFox {
    glm::vec2 at{0.f};
    float secs = 0.f;
  };
  std::vector<AddedFox> addFoxes;
};

void printUsage() {
  std::fprintf(stderr,
               "pxe — contoured terrain and grass\n"
               "  --width N --height N\n"
               "  --capture path.png       the %dx%d low-res view, then exit\n"
               "  --window-png path.png    integer-upscaled window, then exit\n"
               "  --scene ruins|hills|tavern  default ruins; tavern starts inside the Sleeping Fox (in play, at\n"
               "                           its inside door; walking out loads the ruins)\n"
               "  --tavern-door-open       every tavern door shown fully open\n"
               "  --hour H                 time of day, 0-24 (captures default to 15)\n"
               "  --clouds C               cloud cover 0-1 (default 0.35)\n"
               "  --rain R                 rain severity 0-1 (0.2 drizzle, 0.55 rain, 1 downpour)\n"
               "  --no-fire                campfire unlit\n"
               "  --fire-level L,S         captures: campfires burning at L (0 out .. 1), smoke S once out\n"
               "  --shelter-test           ruins: add a fire under a stone shelter and one under a tree\n"
               "  --extra-fires N          ruins: N bare flames over the lawn, to test many fires\n"
               "  --candles                ruins: candles on the stonework and the lawn (and under the --shelter-test roof)\n"
               "  --settle S               captures: run the weather S seconds first (rain puts fires out)\n"
               "  --grass-fire X,Z         set the grass alight at world X,Z (repeatable; G does it at the view's centre)\n"
               "  --rain-from S            with --settle: dry for S seconds, then --rain\n"
               "  --fire-life S            a grass fire spreads for S seconds, then dies out (default 30; 0 never)\n"
               "  --fireball X,Z,DEG[,TURN]  roll a fireball from X,Z pushed toward compass DEG, turning TURN deg/s\n"
               "  --brush KIND,X1,Z1,X2,Z2,T0,T1[,R]  paint a spell brush stroke (fire, water or regrow) from X1,Z1\n"
               "                           to X2,Z2 between T0 and T1 s, R m wide (default 1; repeatable; with\n"
               "                           --ui-shot it goes through the mouse's pick of the ground)\n"
               "                           (play: F launches one, I J K L steer, C follows it)\n"
               "  --no-rays                no god rays\n"
               "  --fireflies              fireflies out\n"
               "  --no-lightning           no lightning flashes in rain and downpour\n"
               "  --mute                   start with sound off\n"
               "  --audio-wav path.wav     render 20 s of weather sound (--rain, --wind-strength) with two thunderclaps, exit\n"
               "  --flash AZ,EL[,LEVEL]    captures: hold a lightning flash from compass AZ, EL degrees up\n"
               "  --wind DEG --wind-strength S   wind direction (compass) and strength (default 20, 1)\n"
               "  --menu                   start with the settings menu open (Tab)\n"
               "  --ui-shot path.png       run the interactive loop briefly, save the window with the menu, exit\n"
               "  --bench N                render N frames uncapped with time running, print ms per frame\n"
               "  --bench-paused           ...with time paused (set PXE_BENCH_PASSES=1 for GPU timers)\n"
               "  --yaw degrees            default from the scene\n"
               "  --slide metres           move the focus along horizontal forward\n"
               "  --target x,y,z\n"
               "  --hero / --no-hero       the hooded hero (on when playing: WASD moves it, Shift walks)\n"
               "  --hero-size S            the hero against a real person (default 1.6)\n"
               "  --hero-move DEG,SECS[,WALK]  captures: run (WALK 1: walk) the hero toward compass DEG first\n"
               "  --hero-attack SECS       captures: the hero SECS into an attack (X; then on toward DEG if moving)\n"
               "  --hero-combo N           ...pressing X N times, 0.2 s apart, to chain the combo\n"
               "  --hero-charge SECS       ...holding X that long first (a charged spin)\n"
               "  --fox / --no-fox         ruins: when playing, a fox asleep by the campfire (--no-fox: none);\n"
               "                           in captures, --fox shows it\n"
               "  --foxes N                ruins: N foxes instead, each bed found clear of the scene and the\n"
               "                           others (the same every run), in play, captures and --bench; prints\n"
               "                           how many fit when fewer than N do\n"
               "  --fox-wake SECS          ...every fox shown SECS into its wake sequence (0 starting to stand;\n"
               "                           7.5 lying back down, or later after a walk), in captures and at the\n"
               "                           start of play; implies --fox\n"
               "  --fox-walk               ...every fox's next wake (--fox-wake's, or in play the first) takes a\n"
               "                           short walk out and back if a route is clear (unforced, about one wake\n"
               "                           in five walks); implies --fox\n"
               "  --add-fox X,Z,SECS       ruins: add a fox as a click with the fox tool (4) at world X,Z would:\n"
               "                           in play SECS s in; in captures before the shot, or with --fox-wake\n"
               "                           SECS s into the foxes' run (repeatable)\n"
               "  --min-dist metres        clump Poisson spacing (default %.4g, the engine's)\n"
               "  --time seconds           wind time for captures (default 0)\n"
               "  --hide-grass --magenta --ndl --test-colours --no-tufts --no-edges\n"
               "  --stamps --no-depth\n"
               "  --self-test              snap stability of the low-res image\n"
               "  --no-vsync\n",
               pxe::RenderConfig::kViewW, pxe::RenderConfig::kViewH,
               static_cast<double>(pxe::GrassScatter{}.clumpSpacing));
}

bool parseTarget(const char* text, glm::vec3& out) {
  float x = 0, y = 0, z = 0;
  if (std::sscanf(text, "%f,%f,%f", &x, &y, &z) != 3) return false;
  out = {x, y, z};
  return true;
}

Options parseArgs(int argc, char** argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto need = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", flag);
        std::exit(1);
      }
      return argv[++i];
    };
    if (a == "--help" || a == "-h") {
      printUsage();
      std::exit(0);
    } else if (a == "--width") {
      opt.width = std::atoi(need("--width"));
    } else if (a == "--height") {
      opt.height = std::atoi(need("--height"));
    } else if (a == "--capture") {
      opt.capture = need("--capture");
    } else if (a == "--window-png") {
      opt.windowPng = need("--window-png");
    } else if (a == "--yaw") {
      opt.yawDeg = std::atof(need("--yaw"));
      opt.haveYaw = true;
    } else if (a == "--scene") {
      opt.scene = need("--scene");
    } else if (a == "--tavern-door-open") {
      opt.tavernDoorOpen = true;
    } else if (a == "--slide") {
      opt.slide = std::atof(need("--slide"));
    } else if (a == "--min-dist") {
      opt.minDist = std::atof(need("--min-dist"));
    } else if (a == "--target") {
      if (!parseTarget(need("--target"), opt.target)) {
        std::fprintf(stderr, "bad --target, expected x,y,z\n");
        std::exit(1);
      }
      opt.haveTarget = true;
    } else if (a == "--hero") {
      opt.hero = 1;
    } else if (a == "--no-hero") {
      opt.hero = 0;
    } else if (a == "--fox") {
      opt.fox = 1;
    } else if (a == "--no-fox") {
      opt.fox = 0;
    } else if (a == "--foxes") {
      opt.foxes = std::max(0, std::atoi(need("--foxes")));
    } else if (a == "--fox-wake") {
      opt.foxWake = std::max(0.f, static_cast<float>(std::atof(need("--fox-wake"))));
    } else if (a == "--fox-walk") {
      opt.foxWalk = true;
    } else if (a == "--add-fox") {
      Options::AddedFox f;
      if (std::sscanf(need("--add-fox"), "%f,%f,%f", &f.at.x, &f.at.y, &f.secs) != 3) {
        std::fprintf(stderr, "bad --add-fox, expected x,z,secs\n");
        std::exit(1);
      }
      f.secs = std::max(0.f, f.secs);
      opt.addFoxes.push_back(f);
    } else if (a == "--hero-size") {
      opt.heroSize = static_cast<float>(std::atof(need("--hero-size")));
    } else if (a == "--hero-charge") {
      opt.heroHold = static_cast<float>(std::atof(need("--hero-charge")));
    } else if (a == "--hero-combo") {
      opt.heroPresses = std::max(1, std::atoi(need("--hero-combo")));
    } else if (a == "--hero-attack") {
      opt.heroAttack = static_cast<float>(std::atof(need("--hero-attack")));
    } else if (a == "--hero-move") {
      if (std::sscanf(need("--hero-move"), "%f,%f,%f", &opt.heroMove.x, &opt.heroMove.y, &opt.heroMove.z) < 2) {
        std::fprintf(stderr, "bad --hero-move, expected DEG,SECS[,WALK]\n");
        std::exit(1);
      }
      opt.haveHeroMove = true;
    } else if (a == "--hide-grass") {
      opt.hideGrass = true;
    } else if (a == "--magenta") {
      opt.magenta = true;
    } else if (a == "--ndl") {
      opt.ndl = true;
    } else if (a == "--test-colours" || a == "--test-colors") {
      opt.testColours = true;
    } else if (a == "--bench") {
      opt.bench = std::atoi(need("--bench"));
    } else if (a == "--bench-paused") {
      opt.benchPaused = true;
    } else if (a == "--ui-shot") {
      opt.uiShot = need("--ui-shot");
    } else if (a == "--menu") {
      opt.menu = true;
    } else if (a == "--hour") {
      opt.hour = std::atof(need("--hour"));
    } else if (a == "--wind") {
      opt.windDegrees = std::atof(need("--wind"));
    } else if (a == "--wind-strength") {
      opt.windStrength = std::atof(need("--wind-strength"));
    } else if (a == "--rain") {
      opt.rain = std::atof(need("--rain"));
    } else if (a == "--no-fire") {
      opt.fire = false;
    } else if (a == "--settle") {
      opt.settle = static_cast<float>(std::atof(need("--settle")));
    } else if (a == "--extra-fires") {
      opt.extraFires = std::atoi(need("--extra-fires"));
    } else if (a == "--grass-fire") {
      glm::vec2 p{0.f};
      if (std::sscanf(need("--grass-fire"), "%f,%f", &p.x, &p.y) != 2) {
        std::fprintf(stderr, "bad --grass-fire, expected x,z\n");
        std::exit(1);
      }
      opt.grassFires.push_back(p);
    } else if (a == "--fireball") {
      if (std::sscanf(need("--fireball"), "%f,%f,%f,%f", &opt.fireball.x, &opt.fireball.y, &opt.fireball.z,
                      &opt.fireball.w) < 3) {
        std::fprintf(stderr, "bad --fireball, expected x,z,heading[,turn]\n");
        std::exit(1);
      }
      opt.haveFireball = true;
    } else if (a == "--brush") {
      Options::ScriptedStroke b;
      char kind[16] = {};
      const int got = std::sscanf(need("--brush"), "%15[a-z],%f,%f,%f,%f,%f,%f,%f", kind, &b.from.x, &b.from.y, &b.to.x,
                                  &b.to.y, &b.start, &b.end, &b.radius);
      b.kind = std::strcmp(kind, "fire") == 0 ? 0 : std::strcmp(kind, "water") == 0 ? 1 : std::strcmp(kind, "regrow") == 0 ? 2 : -1;
      if (got < 7 || b.kind < 0 || b.end < b.start) {
        std::fprintf(stderr, "bad --brush, expected fire|water|regrow,x1,z1,x2,z2,t0,t1[,radius]\n");
        std::exit(1);
      }
      opt.brushes.push_back(b);
    } else if (a == "--fire-life") {
      opt.fireLife = static_cast<float>(std::atof(need("--fire-life")));
    } else if (a == "--rain-from") {
      opt.rainFrom = static_cast<float>(std::atof(need("--rain-from")));
    } else if (a == "--candles") {
      opt.candles = true;
    } else if (a == "--shelter-test") {
      opt.shelterTest = true;
    } else if (a == "--fire-level") {
      std::sscanf(need("--fire-level"), "%f,%f", &opt.fireLevel, &opt.fireSmoke);
    } else if (a == "--no-rays") {
      opt.rays = false;
    } else if (a == "--fireflies") {
      opt.fireflies = true;
    } else if (a == "--no-lightning") {
      opt.lightning = false;
    } else if (a == "--mute") {
      opt.sound = false;
    } else if (a == "--audio-wav") {
      opt.audioWav = need("--audio-wav");
    } else if (a == "--flash") {
      opt.flash = 1.f;
      std::sscanf(need("--flash"), "%f,%f,%f", &opt.flashAz, &opt.flashEl, &opt.flash);
    } else if (a == "--clouds") {
      opt.clouds = std::atof(need("--clouds"));
    } else if (a == "--no-edges") {
      opt.edges = false;
    } else if (a == "--no-tufts") {
      opt.tufts = false;
    } else if (a == "--time") {
      opt.time = std::atof(need("--time"));
    } else if (a == "--stamps") {
      opt.stamps = true;
    } else if (a == "--no-depth") {
      opt.noDepth = true;
    } else if (a == "--self-test") {
      opt.selfTest = true;
    } else if (a == "--no-vsync") {
      opt.vsync = false;
    } else {
      std::fprintf(stderr, "unknown argument %s\n", a.c_str());
      printUsage();
      std::exit(1);
    }
  }
  return opt;
}

struct Input {
  float panX = 0.f;
  float panY = 0.f;
  glm::vec2 move{0.f};  // WASD unscaled: x screen right, y screen up, for the hero
  bool walk = false;    // Shift held
  bool attack = false;  // X (key or pad button) pressed
  bool attackHeld = false;  // and held
  int rotate = 0;
  bool toggleGrass = false;
  bool toggleMagenta = false;
  bool toggleNdl = false;
  bool toggleMenu = false;
  bool cycleRain = false;
  bool strike = false;
  bool toggleSound = false;
  bool togglePause = false;
  bool igniteGrass = false;
  bool launchFireball = false;
  bool follow = false;
  int brush = -2;  // 1 2 3 pick a spell brush (0 fire, 1 water, 2 regrowth), 4 the fox tool (3); -2 none pressed
  glm::vec2 steer{0.f};  // fireball push: x screen right, y screen up
  int speedSteps = 0;  // +1 doubles, -1 halves
  bool shot = false;
  bool quit = false;
};

// This frame's actions from the keys and the pad. While the menu has the
// keyboard only Tab and the pad's Menu button (to close it) are read.
Input pollInput(const pxe::Input& raw, bool keyboardTaken, game::Gamepad& pad) {
  using Key = pxe::Key;
  auto pressed = [&](Key key) { return raw.keyPressed(key); };
  auto down = [&](Key key) { return raw.keyDown(key); };
  Input in;
  if (pressed(Key::kTab)) in.toggleMenu = true;
  pad.poll(raw);
  if (pad.toggleMenu) in.toggleMenu = true;
  if (keyboardTaken) return in;
  if (pad.togglePause) in.togglePause = true;
  if (pressed(Key::kX) || pad.attack) in.attack = true;
  in.attackHeld = down(Key::kX) || pad.attackHeld;
  in.rotate += pad.rotate;
  if (down(Key::kA) || down(Key::kLeft)) in.panX -= 1.f;
  if (down(Key::kD) || down(Key::kRight)) in.panX += 1.f;
  if (down(Key::kW) || down(Key::kUp)) in.panY += 1.f;
  if (down(Key::kS) || down(Key::kDown)) in.panY -= 1.f;
  // The pad's stick adds to the keys; pushed part way, the hero goes at
  // part pace.
  in.move = glm::vec2{in.panX, in.panY} + pad.move;
  if (glm::length(in.move) > 1.f) in.move = glm::normalize(in.move);
  if (in.panX == 0.f && in.panY == 0.f) {
    in.panX = pad.move.x;
    in.panY = pad.move.y;
  }
  in.walk = down(Key::kLeftShift) || down(Key::kRightShift);
  const float speed = in.walk ? 14.f : 6.f;
  in.panX *= speed;
  in.panY *= speed;

  if (pressed(Key::kSpace)) in.togglePause = true;
  if (pressed(Key::kR)) in.cycleRain = true;
  if (pressed(Key::kL)) in.strike = true;
  if (pressed(Key::kM)) in.toggleSound = true;
  if (pressed(Key::kG)) in.igniteGrass = true;
  if (pressed(Key::kF)) in.launchFireball = true;
  if (pressed(Key::k1)) in.brush = 0;
  if (pressed(Key::k2)) in.brush = 1;
  if (pressed(Key::k3)) in.brush = 2;
  if (pressed(Key::k4)) in.brush = game::BrushSettings::kFox;
  if (pressed(Key::kC)) in.follow = true;
  if (down(Key::kL)) in.steer.x += 1.f;
  if (down(Key::kJ)) in.steer.x -= 1.f;
  if (down(Key::kI)) in.steer.y += 1.f;
  if (down(Key::kK)) in.steer.y -= 1.f;
  if (pressed(Key::kLeftBracket)) in.speedSteps -= 1;
  if (pressed(Key::kRightBracket)) in.speedSteps += 1;
  if (pressed(Key::kQ)) in.rotate -= 1;
  if (pressed(Key::kE)) in.rotate += 1;
  if (pressed(Key::kF1)) in.toggleNdl = true;
  if (pressed(Key::kF2)) in.toggleGrass = true;
  if (pressed(Key::kF3)) in.toggleMagenta = true;
  if (pressed(Key::kF4)) in.shot = true;
  if (pressed(Key::kEscape)) in.quit = true;
  return in;
}

int differingPixels(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  int diff = 0;
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i + 2 < n; i += 4) {
    if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) ++diff;
  }
  return diff;
}

// Camera moved +dx texels along screen x, so shifted[x] equals base[x+dx].
int shiftedDiff(const std::vector<std::uint8_t>& base, const std::vector<std::uint8_t>& shifted, int w,
                int h, int dx) {
  int diff = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int bx = x + dx;
      if (bx < 0 || bx >= w) continue;
      const std::size_t ib = (static_cast<std::size_t>(y) * w + bx) * 4;
      const std::size_t is = (static_cast<std::size_t>(y) * w + x) * 4;
      if (base[ib] != shifted[is] || base[ib + 1] != shifted[is + 1] || base[ib + 2] != shifted[is + 2])
        ++diff;
    }
  }
  return diff;
}

constexpr float kCaptureHour = 15.f;

// The fires burning this frame, for the renderer.
std::vector<pxe::FireDraw> fireDraws(const game::WorldClock& clock) {
  std::vector<pxe::FireDraw> draws;
  for (const game::WorldClock::Fire& f : clock.fires) draws.push_back({f.id, f.base, f.drawnLevel(), f.smoke, f.size});
  return draws;
}

// Compass degrees (0 north, -z; 90 east, +x) to a world xz direction.
glm::vec2 compass(float degrees) {
  const float a = degrees * 3.14159265f / 180.f;
  return {std::sin(a), -std::cos(a)};
}

// Which foxes this run shows. None in the hills (buildScene builds the
// ruins for any other name), the self-test or with --no-fox. --foxes N
// places N where they fit, in play, captures and the bench alike. Without
// it, play shows the one hand-placed fox, captures show it only with --fox,
// --fox-wake or --fox-walk, and the bench none.
enum class FoxSet { kNone, kFirst, kFound };
FoxSet foxesShown(const Options& opt) {
  if (opt.scene == "hills" || opt.selfTest || opt.fox == 0) return FoxSet::kNone;
  if (opt.foxes >= 0) return opt.foxes > 0 ? FoxSet::kFound : FoxSet::kNone;
  if (opt.bench > 0) return FoxSet::kNone;
  const bool capture = !opt.capture.empty() || !opt.windowPng.empty();
  if (!capture) return FoxSet::kFirst;
  return opt.fox == 1 || opt.foxWake >= 0.f || opt.foxWalk ? FoxSet::kFirst : FoxSet::kNone;
}

// Every fox on `seconds` together, with --fox-wake. A lone fox takes it in
// one step, as it always did. Several are stopped wherever one of them may
// decide on a walk, so that it decides against the others' beds and routes
// as they are then.
void runFoxes(std::vector<game::Fox>& foxes, const pxe::Scene& scene, float seconds) {
  if (foxes.size() == 1) {
    foxes[0].update(scene, seconds, foxes);
    return;
  }
  while (seconds > 0.f) {
    float step = seconds;
    for (const game::Fox& f : foxes) step = std::min(step, f.untilWalkDecided());
    if (step <= 0.f) step = std::min(seconds, 1e-3f);
    for (game::Fox& f : foxes) f.update(scene, step, foxes);
    seconds -= step;
  }
}

// Whether this run loads the fox model (and keeps the scene's layout for
// finding beds): whenever it shows foxes, and besides in play in the ruins,
// so the fox tool can add them, and in captures with --add-fox. Never in
// the hills, the self-test or (without --foxes) the bench.
bool foxModelWanted(const Options& opt, FoxSet shown) {
  if (shown != FoxSet::kNone) return true;
  if (opt.scene == "hills" || opt.selfTest || opt.bench > 0) return false;
  const bool capture = !opt.capture.empty() || !opt.windowPng.empty();
  return !capture || !opt.addFoxes.empty();
}

// The --add-fox foxes in the order they are added: by SECS, as given where equal.
std::vector<Options::AddedFox> addedFoxes(const Options& opt) {
  std::vector<Options::AddedFox> added = opt.addFoxes;
  std::stable_sort(added.begin(), added.end(),
                   [](const Options::AddedFox& a, const Options::AddedFox& b) { return a.secs < b.secs; });
  return added;
}

// A fox added where the fox tool was clicked, at `at` (world xz) with the
// camera looking along `forward`: the next fox (its seeds by its index), in
// the nearest bed to `at` that fits among the foxes as they are now, its
// curl toward a campfire within 6 m or else turned to stand facing the
// camera, built, spawned asleep at the start of a fresh rest and given the
// fires as every fox is, and the blades and tufts under its curl cleared.
// Logs the fox, or that none fitted; false if none did. Call between frames:
// it adds to `foxes`.
bool addFox(std::vector<game::Fox>& foxes, const pxe::SkinnedModel& model, const game::Fox::Sweep& sweep,
            const game::SceneLayout& layout, pxe::Scene& scene, pxe::Renderer& renderer, glm::vec2 at,
            glm::vec3 forward) {
  game::Fox fox;
  const int index = static_cast<int>(foxes.size());
  fox.build(model, index);
  glm::vec2 toCamera{-forward.x, -forward.z};
  toCamera = glm::length(toCamera) > 1e-4f ? glm::normalize(toCamera) : glm::vec2{0.f, 1.f};
  game::FoxBed bed;
  if (!fox.findBed(sweep, layout, at, toCamera, foxes, bed)) {
    std::fprintf(stderr, "pxe: no room for a fox within %.0f m of %.2f, %.2f\n", game::Fox::kAddReach, at.x, at.y);
    return false;
  }
  fox.spawn(scene, bed.xz, bed.yaw);
  fox.setFires(layout.campfires, game::kCampfireReach);
  // Blades and tufts rooted under the curl drew over it, as under the foxes
  // there from the start; leaves overhead stay. The fire grid keeps the fuel
  // it had.
  const std::size_t before = scene.grass.size();
  std::erase_if(scene.grass, [&](const pxe::GrassInstance& g) {
    return g.kind < 1.5f && glm::length(glm::vec2{g.px, g.pz} - bed.xz) < game::Fox::kFootprint;
  });
  if (scene.grass.size() != before) renderer.uploadGrass(scene);
  foxes.push_back(std::move(fox));
  std::fprintf(stderr, "pxe: fox %d added: bed %.2f, %.2f, yaw %.1f deg, %.2f m from %.2f, %.2f (%zu grass sprites dropped)\n",
               index, bed.xz.x, bed.xz.y, bed.yaw * 180.f / 3.14159265f, glm::distance(bed.xz, at), at.x, at.y,
               before - scene.grass.size());
  return true;
}

// Where the hero may not stand this frame: every fox's body.
std::vector<game::Blocker> foxBodies(const std::vector<game::Fox>& foxes) {
  std::vector<game::Blocker> out;
  for (const game::Fox& f : foxes) out.push_back(f.body());
  return out;
}

// The ruins (or the hills) and what lives in them: the scene, and the
// foxes, with their one model, the scene's layout and (where beds may be
// searched for) the fox's sweep, which stay for the whole run, for adding
// foxes to it. Built once and in place - the foxes keep a pointer to the
// model - at the start, or, starting in the tavern, when the hero first
// walks out of it.
struct Outdoors {
  bool built = false;
  game::BuiltScene world;
  std::optional<pxe::SkinnedModel> foxModel;
  game::Fox::Sweep foxSweep;
  std::vector<game::Fox> foxes;
  std::vector<game::FoxBed> beds;
  game::SceneLayout layout;
};

// Build `out` as `build` asks (the ruins, or the hills). The foxes: their
// one model loaded and their beds settled before the scene is built, so the
// grass under them is only cleared once they are sure to be there; a
// missing model is logged and the game goes on without them. The
// hand-placed fox lies by the campfire, its face toward it; any found by
// --foxes as their beds say. Each is spawned asleep in its bed, its walks
// keeping clear of every campfire. False, logged, if the scene fails to
// build.
bool buildOutdoors(const Options& opt, game::BuildOptions build, Outdoors& out) {
  const FoxSet foxSet = foxesShown(opt);
  if (foxModelWanted(opt, foxSet)) {
    try {
      out.foxModel = game::Fox::loadModel();
      out.layout = game::laidOut(build);
      const bool capture = !opt.capture.empty() || !opt.windowPng.empty();
      if (foxSet == FoxSet::kFound || !capture || !opt.addFoxes.empty()) out.foxSweep = game::Fox::sweep(*out.foxModel);
      if (foxSet == FoxSet::kFound) {
        out.beds = game::Fox::findBeds(out.foxSweep, out.layout, opt.foxes);
        if (static_cast<int>(out.beds.size()) < opt.foxes)
          std::fprintf(stderr, "pxe: only %zu of %d foxes fit\n", out.beds.size(), opt.foxes);
      } else if (foxSet != FoxSet::kNone) {
        auto facingFire = [](glm::vec2 xz) {
          const glm::vec2 toFire = game::kRuinsCampfire - xz;
          return game::FoxBed{xz, std::atan2(toFire.x, toFire.y)};
        };
        out.beds.push_back(facingFire(game::kRuinsFoxSpot));
      }
      out.foxes.resize(out.beds.size());
      for (std::size_t i = 0; i < out.foxes.size(); ++i) out.foxes[i].build(*out.foxModel, static_cast<int>(i));
    } catch (const std::exception& ex) {
      std::fprintf(stderr, "pxe: no fox: %s\n", ex.what());
      out.foxes.clear();
      out.beds.clear();
      out.foxModel.reset();
    }
  }
  for (const game::FoxBed& b : out.beds) build.beds.push_back({b.xz, game::Fox::kFootprint});
  try {
    out.world = game::buildScene(build);
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "scene build failed: %s\n", ex.what());
    return false;
  }
  for (std::size_t i = 0; i < out.foxes.size(); ++i) {
    out.foxes[i].spawn(out.world.scene, out.beds[i].xz, out.beds[i].yaw);
    out.foxes[i].setFires(out.layout.campfires, game::kCampfireReach);
    if (opt.foxWalk) out.foxes[i].forceWalk();
  }
  out.built = true;
  return true;
}

// Indoors, where the camera may look: `target` moved as little as it takes,
// across and up the screen, for the whole room (kRoomMin to kRoomMax) to
// stay in view (or, where the room is wider than the view, for it to stay
// in the middle). Outdoors, `target` as it is.
glm::vec3 framed(const pxe::Scene& scene, glm::vec3 target, const pxe::CameraFrame& view) {
  if (!scene.indoors) return target;
  const glm::vec3 middle = 0.5f * (game::kRoomMin + game::kRoomMax);
  glm::vec2 lo{1e9f}, hi{-1e9f};
  for (int i = 0; i < 8; ++i) {
    const glm::vec3 corner{(i & 1) ? game::kRoomMax.x : game::kRoomMin.x, (i & 2) ? game::kRoomMax.y : game::kRoomMin.y,
                           (i & 4) ? game::kRoomMax.z : game::kRoomMin.z};
    const glm::vec2 s{glm::dot(corner - middle, view.right), glm::dot(corner - middle, view.up)};
    lo = glm::min(lo, s);
    hi = glm::max(hi, s);
  }
  const glm::vec2 half{view.orthoHalfW, view.orthoHalfH};
  const glm::vec3 d = target - middle;
  glm::vec2 s{glm::dot(d, view.right), glm::dot(d, view.up)};
  for (int k = 0; k < 2; ++k) {
    const float from = hi[k] - half[k], to = lo[k] + half[k];
    s[k] = from <= to ? std::clamp(s[k], from, to) : 0.5f * (from + to);
  }
  return middle + view.right * s.x + view.up * s.y + view.forward * glm::dot(d, view.forward);
}

// Where the hero starts in captures, the bench and play: in the tavern at
// its inside door, facing into the room; elsewhere at the view's focus,
// facing the camera.
void heroStart(const pxe::Scene& scene, const pxe::Camera& camera, const pxe::CameraFrame& view, glm::vec2& xz,
               float& yaw) {
  if (scene.indoors) {
    const game::Doorway d = game::tavernDoorway();
    xz = d.arrive;
    yaw = d.arriveYaw;
    return;
  }
  xz = {camera.target().x, camera.target().z};
  yaw = std::atan2(-view.forward.x, -view.forward.z);
}

// The loading screen between the ruins and the tavern stays up at least
// this long, then fades into the scene over kLoadingFade.
constexpr double kLoadingShown = 2.0;
constexpr double kLoadingFade = 0.3;
// Rain heard indoors, against outside. The wind is not heard indoors at all.
constexpr float kIndoorWeather = 0.4f;

// The --fireball roll for captures and the bench: launched on the first
// frame, then pushed along its heading as it turns.
void scriptFireball(const Options& opt, game::Fireball& ball, game::WorldClock& clock, const pxe::Scene& scene,
                    int frame, float dt, std::vector<glm::vec2>& ignite) {
  if (!opt.haveFireball) return;
  if (frame == 0) ball.launch(clock, scene, {opt.fireball.x, opt.fireball.y}, compass(opt.fireball.z));
  ball.update(clock, scene, dt, compass(opt.fireball.z + opt.fireball.w * static_cast<float>(frame) * dt), ignite);
}

// The --brush strokes over the frame from t - dt to t seconds in: the
// stretch each brush covered in that time.
void scriptBrushes(const Options& opt, float t, float dt, std::vector<pxe::FireGrid::Stroke>& out) {
  for (const Options::ScriptedStroke& b : opt.brushes) {
    const float t0 = std::max(t - dt, b.start), t1 = std::min(t, b.end);
    if (t1 < t0) continue;
    auto at = [&](float u) { return b.end > b.start ? glm::mix(b.from, b.to, (u - b.start) / (b.end - b.start)) : b.to; };
    out.push_back({at(t0), at(t1), b.radius, b.kind});
  }
}

float brushesEnd(const Options& opt) {
  float end = -1.f;
  for (const Options::ScriptedStroke& b : opt.brushes) end = std::max(end, b.end);
  return end;
}

// Brushes reach the game's fires (campfires, candles, the fireball) as
// well as the grass fire, which takes the strokes itself: water puts them
// out and fire relights them.
void brushFires(game::WorldClock& clock, const std::vector<pxe::FireGrid::Stroke>& strokes) {
  for (const pxe::FireGrid::Stroke& k : strokes) {
    if (k.kind == pxe::FireGrid::Stroke::kWater) clock.douse(k.from, k.to, k.radius);
    if (k.kind == pxe::FireGrid::Stroke::kFire) clock.kindle(k.from, k.to, k.radius);
  }
}

pxe::FrameParams makeParams(const Options& opt) {
  pxe::FrameParams params;
  params.framebufferW = opt.width;
  params.framebufferH = opt.height;
  params.showGrass = !opt.hideGrass;
  params.magentaGrass = opt.magenta;
  params.showNdl = opt.ndl;
  params.testColours = opt.testColours;
  params.grassDepthTest = !opt.noDepth;
  params.time = opt.time;
  params.edges = opt.edges;
  params.fire = opt.fire;
  params.rays = opt.rays;
  params.fireflies = opt.fireflies;
  params.grassFire = !opt.grassFires.empty() || opt.haveFireball || !opt.brushes.empty();
  params.grassFireLife = opt.fireLife;
  params.present = true;
  return params;
}

int selfTest(pxe::Renderer& renderer, pxe::Camera& camera, const game::BuiltScene& built) {
  const pxe::Scene& scene = built.scene;
  pxe::FrameParams params;
  params.lighting = pxe::lightingAt(scene, kCaptureHour);
  for (std::uint32_t i = 0; i < built.fires.size(); ++i)
    params.fires.push_back({i, built.fires[i].base, 1.f, 0.f, built.fires[i].size});
  params.framebufferW = 320;
  params.framebufferH = 180;
  params.present = false;

  auto grab = [&]() {
    const pxe::CameraFrame frame = camera.frame(renderer.rtW(), renderer.rtH());
    renderer.render(scene, frame, params);
    return std::pair{frame, renderer.readView()};
  };

  auto [frameA, imageA] = grab();
  auto [frameA2, imageA2] = grab();
  const int repeatDiff = differingPixels(imageA, imageA2);

  camera.nudgeImmediate(frameA.right * frameA.texelWorld * 0.25f);
  auto [frameB, imageB] = grab();
  const int subpixelDiff = differingPixels(imageA, imageB);

  camera.nudgeImmediate(frameB.right * frameB.texelWorld * 1.f);
  auto [frameC, imageC] = grab();
  const int panDiff = shiftedDiff(imageA, imageC, renderer.viewW(), renderer.viewH(), 1);

  const bool errorMoved = glm::distance(frameA.texelError, frameB.texelError) > 0.05f;
  const bool pass = repeatDiff == 0 && subpixelDiff == 0 && panDiff == 0 && errorMoved;
  std::printf("self-test: repeat differing pixels = %d (want 0)\n", repeatDiff);
  std::printf("self-test: subpixel differing pixels = %d (want 0)\n", subpixelDiff);
  std::printf("self-test: one-texel realigned differing pixels = %d (want 0)\n", panDiff);
  std::printf("self-test: snap error moved = %s  (%.3f, %.3f) -> (%.3f, %.3f)\n", errorMoved ? "yes" : "no",
              frameA.texelError.x, frameA.texelError.y, frameB.texelError.x, frameB.texelError.y);
  std::printf("self-test: %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}

}  // namespace

// Weather sound rendered offline to a 16-bit WAV: 20 s of wind and rain as
// set on the command line, with a near thunderclap on the left at 2 s and a
// far one on the right at 10 s. Prints the level of each second.
int writeAudioWav(const Options& opt) {
  game::Audio audio;
  game::SoundSettings sound;
  audio.setVolumes(sound);
  audio.setWeather(opt.windStrength, opt.rain);
  constexpr int kSeconds = 20;
  const int rate = game::Audio::kRate;
  std::vector<float> buf(static_cast<std::size_t>(rate) * 2);
  std::vector<float> samples;
  samples.reserve(buf.size() * kSeconds);
  for (int sec = 0; sec < kSeconds; ++sec) {
    if (sec == 2) audio.thunderAt(-0.7f, 0.05f);
    if (sec == 10) audio.thunderAt(0.7f, 0.8f);
    audio.render(buf.data(), rate);
    double sum = 0.0;
    float peak = 0.f;
    for (float v : buf) {
      sum += static_cast<double>(v) * v;
      peak = std::max(peak, std::fabs(v));
    }
    samples.insert(samples.end(), buf.begin(), buf.end());
    std::fprintf(stderr, "audio: %2d s  rms %.3f  peak %.3f\n", sec, std::sqrt(sum / buf.size()), peak);
  }
  if (!pxe::writeWav(opt.audioWav, rate, samples)) return 1;
  std::fprintf(stderr, "pxe: wrote %s\n", opt.audioWav.c_str());
  return 0;
}

int main(int argc, char** argv) {
  const Options opt = parseArgs(argc, argv);
  if (!opt.audioWav.empty()) return writeAudioWav(opt);

  const bool headless = opt.selfTest || !opt.capture.empty() || !opt.windowPng.empty() || opt.bench > 0;
  pxe::WindowOptions windowOptions;
  windowOptions.width = opt.width;
  windowOptions.height = opt.height;
  windowOptions.title = "pxe";
  windowOptions.visible = !headless && opt.uiShot.empty();
  windowOptions.vsync = opt.vsync && !headless;
  std::optional<pxe::Window> window;
  try {
    window.emplace(windowOptions);
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "%s\n", ex.what());
    return 1;
  }

  int exitCode = 0;
  {
    pxe::Renderer renderer;
    try {
      renderer.init();
    } catch (const std::exception& ex) {
      std::fprintf(stderr, "renderer init failed: %s\n", ex.what());
      exitCode = 1;
    }
    if (exitCode == 0) {
      game::BuildOptions build;
      build.minDist = opt.minDist;
      build.tufts = opt.tufts && !opt.stamps;
      build.stamps = opt.stamps;
      build.shelterTest = opt.shelterTest;
      build.extraFires = opt.extraFires;
      build.candles = opt.candles;
      // Starting in the tavern, the ruins are built when the hero first
      // walks out; any other scene now, with its foxes. Either way, what
      // the run starts in is `scene` - and in captures, the bench and the
      // self-test, all it shows.
      const bool startsInside = opt.scene == "tavern";
      build.scene = startsInside ? "ruins" : opt.scene;
      game::BuildOptions tavernBuild;
      tavernBuild.scene = "tavern";
      Outdoors outdoors;
      std::optional<game::BuiltScene> tavern;
      bool ready = true;
      if (startsInside) {
        try {
          tavern.emplace(game::buildScene(tavernBuild));
        } catch (const std::exception& ex) {
          std::fprintf(stderr, "scene build failed: %s\n", ex.what());
          ready = false;
        }
      } else {
        ready = buildOutdoors(opt, build, outdoors);
      }
      if (!ready) {
        return 1;
      }
      game::BuiltScene& built = startsInside ? *tavern : outdoors.world;
      pxe::Scene& scene = built.scene;
      std::vector<game::Fox>& foxes = outdoors.foxes;
      std::optional<pxe::SkinnedModel>& foxModel = outdoors.foxModel;
      renderer.upload(scene);

      // The tavern's two doors, the exterior's in the ruins and the inside
      // door in the tavern, from the one model: wherever the tavern can be
      // (not the hills, nor the self-test). A missing model is logged and the
      // doorways stand open.
      std::optional<pxe::SkinnedModel> doorModel;
      game::TavernDoor outsideDoor, insideDoor;
      if (opt.scene != "hills" && !opt.selfTest) {
        try {
          doorModel = game::TavernDoor::loadModel();
          outsideDoor.build(*doorModel, game::ruinsDoorway());
          insideDoor.build(*doorModel, game::tavernDoorway());
          if (opt.tavernDoorOpen) {
            outsideDoor.holdOpen();
            insideDoor.holdOpen();
          }
        } catch (const std::exception& ex) {
          std::fprintf(stderr, "pxe: no tavern doors: %s\n", ex.what());
          doorModel.reset();
          outsideDoor = game::TavernDoor{};
          insideDoor = game::TavernDoor{};
        }
      }

      pxe::Camera camera;
      camera.setYawRadians((opt.haveYaw ? opt.yawDeg : built.yawDegrees) * 3.14159265f / 180.f);
      camera.setTarget(opt.haveTarget ? opt.target : built.focus);
      if (opt.slide != 0.f) {
        const float yaw = camera.yawRadians();
        const glm::vec3 flatForward{std::sin(yaw), 0.f, std::cos(yaw)};
        camera.setTarget(camera.target() + flatForward * opt.slide);
      }

      // Asleep in their beds (buildOutdoors); or, with --fox-wake, that far
      // into waking (and with --fox-walk, that wake or the first takes a
      // walk). In captures, --add-fox adds foxes first, or with --fox-wake at
      // their moments in the run to it, facing the camera as it starts.
      const bool captureAdds = headless && !opt.selfTest && opt.bench <= 0;
      if (!opt.addFoxes.empty() && (opt.bench > 0 || opt.selfTest))
        std::fprintf(stderr, "pxe: --add-fox is for play and captures; ignored\n");
      else if (!opt.addFoxes.empty() && !foxModel)
        std::fprintf(stderr, "pxe: --add-fox: no fox model (foxes live in the ruins only)\n");
      auto addAt = [&](glm::vec2 at) {
        return addFox(foxes, *foxModel, outdoors.foxSweep, outdoors.layout, scene, renderer, at,
                      camera.frame(renderer.rtW(), renderer.rtH()).forward);
      };
      if (opt.foxWake >= 0.f && !foxes.empty()) {
        for (game::Fox& f : foxes) f.wake(scene, foxes);
        float ran = 0.f;
        if (captureAdds && foxModel) {
          for (const Options::AddedFox& a : addedFoxes(opt)) {
            if (a.secs > opt.foxWake) {
              std::fprintf(stderr, "pxe: the fox added at %.1f s comes after the shot (--fox-wake %.1f): not added\n",
                           a.secs, opt.foxWake);
              continue;
            }
            runFoxes(foxes, scene, a.secs - ran);
            ran = a.secs;
            addAt(a.at);
          }
        }
        runFoxes(foxes, scene, opt.foxWake - ran);
      } else if (captureAdds && foxModel) {
        for (const Options::AddedFox& a : addedFoxes(opt)) addAt(a.at);
      }

      auto draw = [&](const pxe::FrameParams& params) {
        const pxe::CameraFrame frame = camera.frame(renderer.rtW(), renderer.rtH());
        renderer.render(scene, frame, params);
        return frame;
      };
      // A scene's tavern door (none in the hills), and where the hero may not
      // go in it: every fox's body in the ruins, or the room's chairs, stools
      // and near walls in the tavern; and the door's leaf until it is open.
      auto doorOf = [&](const pxe::Scene& s) -> game::TavernDoor& { return s.indoors ? insideDoor : outsideDoor; };
      const std::vector<game::Blocker> roomBlockers = game::tavernBlockers();
      auto blockersIn = [&](const pxe::Scene& s) {
        std::vector<game::Blocker> out = s.indoors ? roomBlockers : foxBodies(foxes);
        doorOf(s).block(out);
        return out;
      };
      auto heroAt = [](const game::Hero& h) {
        return h.built() ? std::optional<glm::vec2>{glm::vec2{h.position().x, h.position().z}} : std::nullopt;
      };

      if (opt.bench > 0) {
        // Same frame as the default window, wind running, timed per pass.
        const glm::ivec2 fbSize = window->framebufferSize();
        const int fbW = fbSize.x, fbH = fbSize.y;
        game::WorldClock clock;
        clock.hour = opt.hour >= 0.f ? opt.hour : kCaptureHour;
        clock.cloudCover = opt.clouds;
        clock.rain = opt.rain;
        clock.lightning = opt.lightning;
        clock.windDegrees = opt.windDegrees;
        clock.cloudSpeed = opt.windStrength;
        clock.placeFires(built.fires);
        game::Fireball ball;
        // With --hero, the hero stands at the view's focus facing the camera
        // (in the tavern, at its inside door), idling, kept out of any foxes
        // (--foxes), which wake and walk as in play.
        game::Hero hero;
        if (opt.hero == 1) {
          hero.build();
          hero.setSize(opt.heroSize);
          glm::vec2 at{0.f};
          float yaw = 0.f;
          heroStart(scene, camera, camera.frame(renderer.rtW(), renderer.rtH()), at, yaw);
          hero.spawn(scene, at, yaw);
        }
        // Per-pass GPU timers block every frame and, on macOS's GL-on-Metal,
        // under-report; they are opt-in. The wall-clock figure is the one to trust.
        const bool passes = std::getenv("PXE_BENCH_PASSES") != nullptr;
        renderer.setProfiling(passes);
        double sum[pxe::PassTimes::kCount] = {};
        const int warmup = 10;
        const double start = window->time();
        double timedStart = start;
        for (int f = 0; f < warmup + opt.bench; ++f) {
          if (f == warmup) timedStart = window->time();
          pxe::FrameParams params = makeParams(opt);
          params.framebufferW = fbW;
          params.framebufferH = fbH;
          params.time = static_cast<float>(f) / 60.f;
          // Advance the day as the game does at default speed, so the sun
          // moves and the shadow map is redrawn every frame (unless paused).
          if (!opt.benchPaused) clock.advance(1.f / 60.f);
          if (f == 0) params.igniteGrass = opt.grassFires;
          scriptFireball(opt, ball, clock, scene, f, 1.f / 60.f, params.igniteGrass);
          scriptBrushes(opt, static_cast<float>(f) / 60.f, 1.f / 60.f, params.gridStrokes);
          brushFires(clock, params.gridStrokes);
          params.lighting = pxe::lightingAt(scene, clock.hour, clock.weather());
          clock.followSun(params.lighting.sunElevationDegrees, 1.f / 60.f);
          params.fires = fireDraws(clock);
          for (game::Fox& fox : foxes) fox.update(scene, 1.f / 60.f, foxes);
          doorOf(scene).update(1.f / 60.f, heroAt(hero));
          hero.update(scene, 1.f / 60.f, game::HeroInput{}, blockersIn(scene));
          hero.pose(params.actors, params.actorEffects, params.actorMaterials);
          for (const game::Fox& fox : foxes) fox.pose(params.actors, params.actorMaterials);
          doorOf(scene).pose(params.actors, params.actorMaterials);
          draw(params);
          window->swap();
          if (f >= warmup)
            for (int i = 0; i < pxe::PassTimes::kCount; ++i) sum[i] += renderer.passTimes().ms[i];
        }
        const double wall = (window->time() - timedStart) / opt.bench * 1000.0;
        if (passes) {
          for (int i = 0; i < pxe::PassTimes::kCount; ++i)
            std::fprintf(stderr, "bench: %-10s %6.2f ms (GPU timer)\n", pxe::PassTimes::name(i), sum[i] / opt.bench);
        }
        std::fprintf(stderr, "bench: wall %6.2f ms/frame (%.0f fps uncapped), %dx%d\n", wall, 1000.0 / wall, fbW, fbH);
      } else if (opt.selfTest) {
        exitCode = selfTest(renderer, camera, built);
      } else if (headless) {
        const glm::ivec2 fbSize = window->framebufferSize();
        const int fbW = fbSize.x, fbH = fbSize.y;
        game::WorldClock clock;
        clock.hour = opt.hour >= 0.f ? opt.hour : kCaptureHour;
        clock.cloudCover = opt.clouds;
        clock.rain = opt.rain;
        clock.lightning = opt.lightning;
        clock.placeFires(built.fires);
        game::Fireball ball;
        for (game::WorldClock::Fire& f : clock.fires) {
          f.level = opt.fireLevel;
          f.smoke = opt.fireSmoke;
          f.lit = opt.fireLevel > 0.f;
        }
        if (opt.settle > 0.f) {
          // The fires burn down (or catch) as they would in play, every frame
          // drawn from the capture's time on; the hour, clouds and lightning
          // stay as asked.
          const bool lightning = clock.lightning;
          clock.paused = true;
          clock.lightning = false;
          const int frames = static_cast<int>(std::ceil(opt.settle * 60.f));
          for (int f = 0; f < frames; ++f) {
            if (opt.rainFrom >= 0.f) clock.rain = static_cast<float>(f) / 60.f < opt.rainFrom ? 0.f : opt.rain;
            clock.advance(1.f / 60.f);
            pxe::FrameParams params = makeParams(opt);
            params.framebufferW = fbW;
            params.framebufferH = fbH;
            params.time = opt.time + static_cast<float>(f) / 60.f;
            if (f == 0) params.igniteGrass = opt.grassFires;
            scriptFireball(opt, ball, clock, scene, f, 1.f / 60.f, params.igniteGrass);
            scriptBrushes(opt, static_cast<float>(f) / 60.f, 1.f / 60.f, params.gridStrokes);
            brushFires(clock, params.gridStrokes);
            params.lighting = pxe::lightingAt(scene, clock.hour, clock.weather());
            clock.followSun(params.lighting.sunElevationDegrees, 1.f / 60.f);
            params.fires = fireDraws(clock);
            draw(params);
          }
          clock.paused = false;
          clock.lightning = lightning;
          for (const game::WorldClock::Fire& f : clock.fires)
            std::fprintf(stderr, "pxe: fire %s, level %.2f, smoke %.2f\n", f.lit ? "lit" : "out", f.level, f.smoke);
          if (opt.haveFireball)
            std::fprintf(stderr, "pxe: fireball %s at %.2f, %.2f, %.2f\n", ball.alive() ? "rolling" : "gone", ball.position().x,
                         ball.position().y, ball.position().z);
        }
        if (opt.flash > 0.f) {
          // Compass bearing, like the wind: 0 north (-z), 90 east (+x).
          const float az = opt.flashAz * 3.14159265f / 180.f, el = opt.flashEl * 3.14159265f / 180.f;
          clock.flash = opt.flash;
          clock.flashDir = {std::cos(el) * std::sin(az), std::sin(el), -std::cos(el) * std::cos(az)};
        }
        clock.windDegrees = opt.windDegrees;
        clock.cloudSpeed = opt.windStrength;
        // The moment shown: --time, after any --settle.
        const float shotTime = opt.settle > 0.f ? opt.time + std::ceil(opt.settle * 60.f) / 60.f : opt.time;
        // The foxes asleep, their breath as of then; or as --fox-wake put
        // them at spawn. The hero moves round them as they are.
        if (opt.foxWake < 0.f)
          for (game::Fox& fox : foxes) fox.holdRest(shotTime);
        // The scene's tavern door swings for the hero, frame by frame, as it
        // moves.
        game::TavernDoor& door = doorOf(scene);
        game::Hero hero;
        if (opt.hero == 1 || opt.haveHeroMove || opt.heroAttack >= 0.f) {
          // Stands at the view's focus facing the camera (in the tavern, at
          // its inside door), then runs or walks the way asked, the view
          // following.
          hero.build();
          hero.setSize(opt.heroSize);
          glm::vec2 at{0.f};
          float yaw = 0.f;
          heroStart(scene, camera, camera.frame(renderer.rtW(), renderer.rtH()), at, yaw);
          hero.spawn(scene, at, yaw);
          const int frames = opt.haveHeroMove ? static_cast<int>(std::lround(opt.heroMove.y * 60.f)) : 0;
          for (int f = 0; f < frames; ++f) {
            game::HeroInput in;
            in.move = compass(opt.heroMove.x);
            in.walk = opt.heroMove.z > 0.5f;
            door.update(1.f / 60.f, heroAt(hero));
            hero.update(scene, 1.f / 60.f, in, blockersIn(scene));
          }
          if (opt.heroAttack >= 0.f) {
            const int attackFrames = static_cast<int>(std::lround(opt.heroAttack * 60.f));
            for (int f = 0; f <= attackFrames; ++f) {
              game::HeroInput in;
              if (opt.haveHeroMove) in.move = compass(opt.heroMove.x);
              in.walk = opt.heroMove.z > 0.5f;
              in.attack = f % 12 == 0 && f / 12 < opt.heroPresses;
              in.attackHeld = in.attack || static_cast<float>(f) / 60.f < opt.heroHold;
              door.update(1.f / 60.f, heroAt(hero));
              hero.update(scene, 1.f / 60.f, in, blockersIn(scene));
              // The smear samples the blade as it is drawn; keep it fed.
              if (f < attackFrames) {
                std::vector<pxe::ActorDraw> a;
                std::vector<const pxe::Mesh*> e;
                pxe::ActorMaterial m[pxe::kActorMaterials];
                hero.pose(a, e, m);
              }
            }
          }
          if (frames > 0 || opt.heroAttack >= 0.f) std::fprintf(stderr, "pxe: hero at %.2f %.2f %.2f, %.2f m/s\n", hero.position().x,
                                       hero.position().y, hero.position().z, hero.speed());
          camera.setTarget(framed(scene, hero.position() + glm::vec3{0.f, hero.size(), 0.f},
                                  camera.frame(renderer.rtW(), renderer.rtH())));
        }
        pxe::FrameParams params = makeParams(opt);
        params.framebufferW = fbW;
        params.framebufferH = fbH;
        hero.pose(params.actors, params.actorEffects, params.actorMaterials);
        door.pose(params.actors, params.actorMaterials);
        if (opt.settle > 0.f) {
          params.time = opt.time + std::ceil(opt.settle * 60.f) / 60.f;
          scriptFireball(opt, ball, clock, scene, static_cast<int>(std::ceil(opt.settle * 60.f)), 1.f / 60.f,
                         params.igniteGrass);
          scriptBrushes(opt, std::ceil(opt.settle * 60.f) / 60.f, 1.f / 60.f, params.gridStrokes);
          brushFires(clock, params.gridStrokes);
        } else {
          params.igniteGrass = opt.grassFires;
          scriptFireball(opt, ball, clock, scene, 0, 1.f / 60.f, params.igniteGrass);
          scriptBrushes(opt, 0.f, 1.f / 60.f, params.gridStrokes);
          brushFires(clock, params.gridStrokes);
        }
        for (const game::Fox& fox : foxes) fox.pose(params.actors, params.actorMaterials);
        params.lighting = pxe::lightingAt(scene, clock.hour, clock.weather());
        // After a settle the night-only fires are where it left them;
        // otherwise as the hour has them.
        clock.followSun(params.lighting.sunElevationDegrees, opt.settle > 0.f ? 1.f / 60.f : -1.f);
        params.fires = fireDraws(clock);
        const pxe::CameraFrame frame = draw(params);
        std::fprintf(stderr, "pxe: snap error %.4f %.4f\n", frame.texelError.x, frame.texelError.y);
        bool ok = true;
        if (!opt.capture.empty()) {
          ok = pxe::writePng(opt.capture, renderer.viewW(), renderer.viewH(),
                        renderer.readView()) &&
               ok;
          std::fprintf(stderr, "pxe: %s %s\n", ok ? "wrote" : "failed", opt.capture.c_str());
        }
        if (!opt.windowPng.empty()) {
          const glm::ivec2 fbSize = window->framebufferSize();
          const int fbW = fbSize.x, fbH = fbSize.y;
          const bool wrote = pxe::writePng(opt.windowPng, fbW, fbH, renderer.readPresented(fbW, fbH));
          std::fprintf(stderr, "pxe: %s %s (%dx%d)\n", wrote ? "wrote" : "failed", opt.windowPng.c_str(),
                       fbW, fbH);
          ok = wrote && ok;
        }
        exitCode = ok ? 0 : 1;
      } else {
        game::ViewToggles view;
        view.grass = !opt.hideGrass;
        view.edges = opt.edges;
        view.fire = opt.fire;
        view.rays = opt.rays;
        view.fireflies = opt.fireflies;
        bool magenta = opt.magenta;
        bool ndl = opt.ndl;
        game::WorldClock clock;
        if (opt.hour >= 0.f) clock.hour = opt.hour;
        clock.cloudCover = opt.clouds;
        clock.rain = opt.rain;
        clock.lightning = opt.lightning;
        clock.windDegrees = opt.windDegrees;
        clock.cloudSpeed = opt.windStrength;
        clock.grassFireLife = opt.fireLife;
        clock.placeFires(built.fires);
        game::SoundSettings sound;
        sound.on = opt.sound;
        game::Audio audio;
        if (!audio.start()) std::fprintf(stderr, "pxe: no audio output, running silent\n");
        int heardStrikes = 0;
        std::optional<pxe::Overlay> overlay;
        try {
          overlay.emplace(*window);
        } catch (const std::exception& ex) {
          std::fprintf(stderr, "%s\n", ex.what());
          return 1;
        }
        game::SettingsMenu menu;
        menu.init(*overlay);
        if (opt.menu) menu.toggle();
        // Averaged over a whole second, so a slow first frame or a hitch
        // does not skew it; with vsync on it tops out at the display rate.
        float fps = 0.f;
        float fpsTime = 0.f;
        int fpsFrames = 0;
        int frameCount = 0;
        bool grassFire = !opt.grassFires.empty() || opt.haveFireball;
        std::vector<glm::vec2> ignite = opt.grassFires;
        game::Fireball ball;
        bool following = false;
        game::BrushSettings brush;
        bool painting = false;      // the left button was painting last frame
        glm::vec2 lastPaintPx{0.f};  // and where the cursor was, framebuffer pixels
        float pickError = 0.f;       // scripted strokes: worst miss of the pick, metres
        glm::vec2 scriptedCursor{-1.f};  // --ui-shot: the brush stays where the script left it
        // --ui-shot: the frame to save; with --brush, once the strokes are done.
        const int uiFrames = opt.brushes.empty() ? 8 : static_cast<int>(std::ceil((brushesEnd(opt) + 1.f) * 60.f));
        // Indoors the camera keeps the room's yaw, whatever --yaw asks.
        if (scene.indoors) camera.setYawRadians(glm::radians(built.yawDegrees));
        pxe::CameraFrame lastFrame = camera.frame(renderer.rtW(), renderer.rtH());
        // The hero stands at the view's focus facing the camera (in the
        // tavern, at its inside door); WASD moves it (Shift walks) and the
        // view follows it.
        game::Hero hero;
        if (opt.hero != 0) {
          try {
            hero.build();
            hero.setSize(opt.heroSize);
            glm::vec2 at{0.f};
            float yaw = 0.f;
            heroStart(scene, camera, lastFrame, at, yaw);
            hero.spawn(scene, at, yaw);
            if (scene.indoors) camera.setTarget(framed(scene, hero.position() + glm::vec3{0.f, hero.size(), 0.f}, lastFrame));
          } catch (const std::exception& ex) {
            std::fprintf(stderr, "pxe: no hero: %s\n", ex.what());
          }
        }
        if (opt.haveFireball && !startsInside) {
          ball.launch(clock, scene, {opt.fireball.x, opt.fireball.y}, compass(opt.fireball.z));
          following = true;
        }
        // The fox tool: a press of the left button (not a hold, and not over
        // the menu) asks for a fox where it picks the ground, as --add-fox
        // does at its moments. Each joins between frames, at the start of the
        // next; with no room, "No room for a fox here" shows by where it was
        // asked for, for kNoRoomShown seconds.
        struct FoxAsk {
          glm::vec2 at;       // world xz
          glm::vec2 shownAt;  // window points
        };
        std::vector<FoxAsk> foxAsks;
        const std::vector<Options::AddedFox> scriptedFoxes = addedFoxes(opt);
        std::size_t nextScriptedFox = 0;
        float played = 0.f;      // seconds since the start, for --add-fox
        constexpr float kNoRoomShown = 2.f;
        float noRoomFor = 0.f;
        glm::vec2 noRoomAt{0.f};
        game::Gamepad gamepad;

        // Inside the tavern, or out in the ruins. Walking into the zone
        // through an open doorway takes the hero to the other, behind the
        // loading screen (a Passage): it comes up and is drawn alone for a
        // frame, the other scene is loaded behind it the next, and it stays
        // up for at least kLoadingShown seconds from when it came up, then
        // fades into the scene over kLoadingFade. The ruins stay as they are
        // while the hero is inside - their foxes, fireball and fires held
        // where they were, and the camera's yaw put by - and come back as
        // they were left, but for the lawn's grass fire, which is put out on
        // the way in (the grass it burnt stays burnt).
        bool inside = startsInside;
        const game::Doorway ruinsWay = game::ruinsDoorway(), tavernWay = game::tavernDoorway();
        float outdoorYaw = camera.restingYawRadians();
        std::vector<game::WorldClock::Fire> outdoorFires;
        std::uint32_t outdoorNextFire = 0;
        struct Passage {
          bool active = false;
          bool loaded = false;    // the other scene is up behind the screen
          int framesUp = 0;       // frames drawn with the screen up
          double since = 0.0;     // when the screen came up
          double loadedAt = 0.0;  // and when the other scene was loaded
          int lore = 0;           // the passage shown, from kTavernLore
        };
        Passage passage;
        std::mt19937 loreRng(std::random_device{}());
        std::uniform_int_distribution<int> pickLore(0, game::kTavernLoreCount - 1);
        bool wasInZone = false;  // the hero was in the zone last frame
        // Into the tavern: built the first time; the ruins' camera yaw and
        // fires put by and the lawn's grass fire put out; the hero just
        // inside the inside door, facing into the room, the camera at the
        // room's yaw. If the room fails to build, it is logged and the hero
        // stays out.
        auto goInside = [&]() {
          if (!tavern) {
            try {
              tavern.emplace(game::buildScene(tavernBuild));
            } catch (const std::exception& ex) {
              std::fprintf(stderr, "pxe: the tavern failed to build: %s\n", ex.what());
              return;
            }
          }
          outdoorYaw = camera.restingYawRadians();
          outdoorFires = clock.fires;
          outdoorNextFire = clock.nextFireId;
          renderer.quenchGrassFire();
          ignite.clear();
          following = false;
          painting = false;
          // The lawn's burnt grass is kept for the way back out: no grass
          // fire runs indoors.
          renderer.upload(tavern->scene, true);
          clock.placeFires(tavern->fires);
          hero.spawn(tavern->scene, tavernWay.arrive, tavernWay.arriveYaw);
          camera.setYawRadians(glm::radians(tavern->yawDegrees));
          camera.setTarget(framed(tavern->scene, hero.position() + glm::vec3{0.f, hero.size(), 0.f},
                                  camera.frame(renderer.rtW(), renderer.rtH())));
          inside = true;
        };
        // Out to the ruins: built the first time (when play started in the
        // tavern), otherwise as they were left, with their fires and the
        // camera's yaw; the hero on the lawn outside the door, facing away
        // from the tavern. If they fail to build, it is logged and the hero
        // stays in.
        auto goOutside = [&]() {
          if (!outdoors.built) {
            if (!buildOutdoors(opt, build, outdoors)) return;
            clock.placeFires(outdoors.world.fires);
            outdoorYaw = glm::radians(outdoors.world.yawDegrees);
          } else {
            clock.fires = outdoorFires;
            clock.nextFireId = outdoorNextFire;
          }
          // Its grass fire's burnt grass as it was left.
          renderer.upload(outdoors.world.scene, true);
          hero.spawn(outdoors.world.scene, ruinsWay.arrive, ruinsWay.arriveYaw);
          camera.setYawRadians(outdoorYaw);
          camera.setTarget(hero.position() + glm::vec3{0.f, hero.size(), 0.f});
          inside = false;
        };

        pxe::Input rawInput;
        double previous = window->time();
        while (!window->shouldClose()) {
          const double now = window->time();
          float dt = static_cast<float>(now - previous);
          previous = now;
          fpsTime += dt;
          ++fpsFrames;
          if (fpsTime >= 1.f) {
            fps = static_cast<float>(fpsFrames) / fpsTime;
            fpsTime = 0.f;
            fpsFrames = 0;
          }
          dt = std::clamp(dt, 0.f, 0.05f);
          window->pollEvents();
          rawInput.update(*window);
          Input input = pollInput(rawInput, menu.wantsKeyboard(), gamepad);
          if (input.quit) window->requestClose();
          if (input.toggleMenu) menu.toggle();
          if (input.togglePause) clock.paused = !clock.paused;
          if (input.brush >= 0) brush.kind = brush.kind == input.brush ? game::BrushSettings::kNone : input.brush;
          // Through a doorway: the frame after the loading screen came up, the
          // other scene is loaded behind it.
          if (passage.active && !passage.loaded && passage.framesUp > 0) {
            if (inside) goOutside();
            else goInside();
            passage.loaded = true;
            passage.loadedAt = window->time();
            wasInZone = false;
            // The view as it now is, for steering and framing until the next
            // frame is drawn.
            lastFrame = camera.frame(renderer.rtW(), renderer.rtH());
          }
          pxe::Scene& here = inside ? tavern->scene : outdoors.world.scene;
          game::TavernDoor& door = doorOf(here);
          // While the loading screen is up the hero stands where it is; and
          // indoors the camera keeps the room's yaw, and the brushes, the
          // fireball and setting the grass alight do nothing.
          if (passage.active) {
            input.move = glm::vec2{0.f};
            input.panX = input.panY = 0.f;
            input.attack = input.attackHeld = false;
          }
          if (inside || passage.active) {
            input.rotate = 0;
            input.igniteGrass = input.launchFireball = input.follow = false;
            input.steer = glm::vec2{0.f};
          }
          // The foxes asked for last frame join now, before any fox moves.
          for (const FoxAsk& ask : foxAsks) {
            if (!foxModel) {
              std::fprintf(stderr, "pxe: no fox added at %.2f, %.2f: no fox model (foxes live in the ruins only)\n",
                           ask.at.x, ask.at.y);
              continue;
            }
            if (!addFox(foxes, *foxModel, outdoors.foxSweep, outdoors.layout, outdoors.world.scene, renderer, ask.at,
                        lastFrame.forward)) {
              noRoomFor = kNoRoomShown;
              noRoomAt = ask.shownAt;
            }
          }
          foxAsks.clear();
          noRoomFor = std::max(0.f, noRoomFor - dt);
          played += dt;
          if (input.strike) clock.strike();
          if (input.toggleSound) sound.on = !sound.on;
          if (input.igniteGrass) {
            grassFire = true;
            ignite.push_back({camera.target().x, camera.target().z});
          }
          if (input.cycleRain) clock.rain = clock.rain <= 0.f ? 0.2f : clock.rain < 0.5f ? 0.55f : clock.rain < 0.9f ? 1.f : 0.f;
          if (input.speedSteps)
            clock.hoursPerMinute = std::clamp(clock.hoursPerMinute * std::pow(2.f, static_cast<float>(input.speedSteps)),
                                              1.f / 60.f, 120.f);
          clock.advance(dt);
          // The foxes first, so the hero is kept out of where they are now;
          // with the hero inside, they hold still.
          if (!inside)
            for (game::Fox& fox : foxes) fox.update(here, dt, foxes);
          // The fireball: F sets one rolling from the middle of the view,
          // away up the screen; I J K L push it up, left, down and right on
          // screen; the view follows it (C toggles) until you pan away. With
          // the hero inside, it holds still.
          {
            glm::vec2 right{lastFrame.right.x, lastFrame.right.z}, up{lastFrame.forward.x, lastFrame.forward.z};
            if (glm::length(right) > 1e-4f) right = glm::normalize(right);
            if (glm::length(up) > 1e-4f) up = glm::normalize(up);
            if (!inside) {
              if (input.launchFireball) {
                ball.launch(clock, here, {camera.target().x, camera.target().z}, up);
                grassFire = true;
                following = true;
              }
              if (input.follow) following = !following;
              if (!hero.built() && (input.panX != 0.f || input.panY != 0.f)) following = false;
              ball.update(clock, here, dt, right * input.steer.x + up * input.steer.y, ignite);
              if (ball.alive() && following) camera.follow(ball.position());
            }
            // The scene's door swings for the hero before it moves, so the
            // hero meets the door as it is now.
            door.update(dt, heroAt(hero));
            if (hero.built()) {
              game::HeroInput in;
              in.move = right * input.move.x + up * input.move.y;
              if (glm::length(in.move) > 1.f) in.move = glm::normalize(in.move);
              in.walk = input.walk;
              in.attack = input.attack;
              in.attackHeld = input.attackHeld;
              hero.update(here, dt, in, blockersIn(here));
              if (glm::length(in.move) > 0.f) following = false;
              if (!(ball.alive() && following))
                camera.follow(framed(here, hero.position() + glm::vec3{0.f, hero.size(), 0.f}, lastFrame));
              input.panX = input.panY = 0.f;
            }
          }
          // Walking into the doorway's zone from outside it starts the way
          // through to the other scene.
          if (!passage.active && hero.built()) {
            const bool inZone = (inside ? tavernWay : ruinsWay).inZone({hero.position().x, hero.position().z});
            if (inZone && !wasInZone) {
              passage = Passage{};
              passage.active = true;
              passage.since = window->time();
              passage.lore = pickLore(loreRng);
            }
            wasInZone = inZone;
          }
          if (input.toggleGrass) view.grass = !view.grass;
          if (input.toggleMagenta) magenta = !magenta;
          if (input.toggleNdl) ndl = !ndl;
          camera.setPanVelocity({input.panX, input.panY});
          if (input.rotate) camera.rotateSteps(input.rotate);
          camera.update(dt);

          const glm::ivec2 fbSize = window->framebufferSize();
          const int fbW = fbSize.x, fbH = fbSize.y;
          // A minimised window can have a 0x0 framebuffer (GLFW on Windows),
          // and render throws when asked to present at that size. The world
          // keeps going and so does its sound, but nothing is drawn, the
          // menu included, until the window has a size again; the wait
          // keeps the loop from spinning flat out meanwhile.
          if (fbW <= 0 || fbH <= 0) {
            audio.setVolumes(sound);
            audio.setWeather(clock.cloudSpeed, clock.rain);
            if (clock.strikes != heardStrikes) {
              heardStrikes = clock.strikes;
              audio.thunder(glm::dot(clock.flashDir, lastFrame.right) /
                            std::max(glm::length(glm::vec2{clock.flashDir.x, clock.flashDir.z}), 1e-3f));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
          }

          // Spell brushes: the ground under the cursor, picked through the
          // last frame drawn. Held down, the left button paints from where
          // the cursor was last frame to where it is now. Scripted --brush
          // strokes go through the same pick, from where their ends show.
          // Out in the ruins only, and not while the loading screen is up.
          std::vector<pxe::FireGrid::Stroke> strokes;
          std::vector<glm::vec2> brushOutline;
          if (!inside && !passage.active) {
            const int winW = window->windowSize().x;
            const float pointScale = winW > 0 ? static_cast<float>(fbW) / static_cast<float>(winW) : 1.f;
            auto pick = [&](glm::vec2 px, glm::vec3& hit) {
              return here.ground.raycast(renderer.rayThrough(lastFrame, fbW, fbH, px), lastFrame.forward, 400.f, hit);
            };
            auto shown = [&](glm::vec2 xz) {
              const float y = here.ground.heightAt(xz);
              return renderer.windowPoint(lastFrame, fbW, fbH, {xz.x, y > pxe::HeightMap::kNone ? y : 0.f, xz.y});
            };
            auto paint = [&](glm::vec2 fromPx, glm::vec2 toPx, float radius, int kind) {
              glm::vec3 a{0.f}, b{0.f};
              if (!pick(fromPx, a) || !pick(toPx, b)) return false;
              strokes.push_back({{a.x, a.z}, {b.x, b.z}, radius, kind});
              grassFire = true;
              return true;
            };
            glm::vec2 cursorPx{-1.f};
            float cursorRadius = brush.radius;
            int cursorKind = brush.kind;
            const bool leftDown = rawInput.mouseDown(pxe::MouseButton::kLeft);
            if (brush.kind == game::BrushSettings::kFox) {
              // A ring as wide as a curl; whether a fox fits is only looked
              // into on a click.
              cursorPx = rawInput.mousePosition();
              cursorRadius = game::Fox::kFootprint;
              glm::vec3 hit{0.f};
              if (rawInput.mousePressed(pxe::MouseButton::kLeft) && !menu.wantsMouse() && pick(cursorPx, hit))
                foxAsks.push_back({{hit.x, hit.z}, cursorPx / pointScale});
              painting = false;
            } else if (brush.kind != game::BrushSettings::kNone) {
              cursorPx = rawInput.mousePosition();
              const bool down = leftDown && !menu.wantsMouse();
              if (down) paint(painting ? lastPaintPx : cursorPx, cursorPx, brush.radius, brush.kind);
              painting = down;
              lastPaintPx = cursorPx;
            } else {
              painting = false;
            }
            while (nextScriptedFox < scriptedFoxes.size() && scriptedFoxes[nextScriptedFox].secs <= played) {
              const glm::vec2 at = scriptedFoxes[nextScriptedFox++].at;
              foxAsks.push_back({at, shown(at) / pointScale});
            }
            std::vector<pxe::FireGrid::Stroke> scripted;
            scriptBrushes(opt, static_cast<float>(frameCount) / 60.f, 1.f / 60.f, scripted);
            for (const pxe::FireGrid::Stroke& k : scripted) {
              if (!paint(shown(k.from), shown(k.to), k.radius, k.kind)) continue;
              pickError = std::max({pickError, glm::distance(strokes.back().from, k.from), glm::distance(strokes.back().to, k.to)});
              // The script holds the brush as a player would.
              scriptedCursor = shown(k.to);
              brush.kind = cursorKind = k.kind;
              brush.radius = cursorRadius = k.radius;
            }
            if (!opt.uiShot.empty() && scriptedCursor.x >= 0.f) cursorPx = scriptedCursor;
            // The brush's outline on the ground round the cursor. It keeps to
            // the cursor's height where the ground steps up onto stone, so it
            // stays a ring on the lawn rather than climbing walls.
            glm::vec3 hit{0.f};
            if (cursorKind != game::BrushSettings::kNone && cursorPx.x >= 0.f && pick(cursorPx, hit)) {
              for (int i = 0; i < 40; ++i) {
                const float a = static_cast<float>(i) / 40.f * 6.2831853f;
                const glm::vec2 xz = glm::vec2{hit.x, hit.z} + cursorRadius * glm::vec2{std::cos(a), std::sin(a)};
                float y = here.ground.heightAt(xz);
                if (y <= pxe::HeightMap::kNone || std::fabs(y - hit.y) > 0.4f) y = hit.y;
                brushOutline.push_back(renderer.windowPoint(lastFrame, fbW, fbH, {xz.x, y, xz.y}) / pointScale);
              }
            }
          } else {
            painting = false;
          }

          pxe::FrameParams params = makeParams(opt);
          params.framebufferW = fbW;
          params.framebufferH = fbH;
          params.showGrass = view.grass;
          params.edges = view.edges;
          params.fire = view.fire;
          params.rays = view.rays;
          params.fireflies = view.fireflies;
          params.lighting = pxe::lightingAt(here, clock.hour, clock.weather());
          clock.followSun(params.lighting.sunElevationDegrees, dt);
          params.fires = fireDraws(clock);
          // The lawn's grass fire runs only while the hero is out on it; it
          // keeps its burnt grass while the hero is inside.
          params.grassFire = grassFire && !inside;
          params.igniteGrass = std::move(ignite);
          ignite.clear();
          brushFires(clock, strokes);
          params.gridStrokes = std::move(strokes);
          params.grassFireLife = clock.grassFireLife;
          params.magentaGrass = magenta;
          params.showNdl = ndl;
          params.time = static_cast<float>(now);
          hero.pose(params.actors, params.actorEffects, params.actorMaterials);
          if (!inside)
            for (const game::Fox& fox : foxes) fox.pose(params.actors, params.actorMaterials);
          door.pose(params.actors, params.actorMaterials);
          // The scene, unless the loading screen is up over the old one, which
          // it hides entirely, while the new one loads.
          if (!passage.active || passage.loaded) {
            lastFrame = camera.frame(renderer.rtW(), renderer.rtH());
            renderer.render(here, lastFrame, params);
          }
          // Weather sound follows the settings: indoors the rain is quieter
          // and the wind silent; each new flash queues its thunder, panned
          // toward the side of the screen it lit from.
          game::SoundSettings heard = sound;
          if (inside) {
            heard.wind = 0.f;
            heard.rain *= kIndoorWeather;
          }
          audio.setVolumes(heard);
          audio.setWeather(clock.cloudSpeed, clock.rain);
          if (clock.strikes != heardStrikes) {
            heardStrikes = clock.strikes;
            audio.thunder(glm::dot(clock.flashDir, lastFrame.right) /
                          std::max(glm::length(glm::vec2{clock.flashDir.x, clock.flashDir.z}), 1e-3f));
          }
          if (input.shot) {
            const bool ok = pxe::writePng("shot.png", renderer.viewW(), renderer.viewH(),
                                     renderer.readView());
            std::fprintf(stderr, "pxe: %s shot.png\n", ok ? "wrote" : "failed");
          }
          // The loading screen: fully up until the other scene is loaded and
          // kLoadingShown seconds have passed since it came up, then fading.
          game::LoadingScreen loading;
          if (passage.active) {
            const double fadeFrom = std::max(passage.since + kLoadingShown, passage.loadedAt);
            loading.title = "The Sleeping Fox";
            loading.passage = game::kTavernLore[passage.lore];
            loading.opacity =
                passage.loaded ? static_cast<float>(std::clamp(1.0 - (window->time() - fadeFrom) / kLoadingFade, 0.0, 1.0)) : 1.f;
          }
          menu.draw(clock, view, sound, brush, brushOutline, noRoomFor > 0.f ? "No room for a fox here" : nullptr, noRoomAt,
                    fps, passage.active ? &loading : nullptr);
          if (passage.active) {
            ++passage.framesUp;
            if (passage.loaded && loading.opacity <= 0.f) passage.active = false;
          }
          ++frameCount;
          if (!opt.uiShot.empty() && frameCount == uiFrames) {
            const bool ok = pxe::writePng(opt.uiShot, fbW, fbH, renderer.readPresented(fbW, fbH));
            std::fprintf(stderr, "pxe: %s %s (%dx%d)\n", ok ? "wrote" : "failed", opt.uiShot.c_str(), fbW, fbH);
            if (!opt.brushes.empty()) std::fprintf(stderr, "pxe: brush picks within %.3f m of the scripted strokes\n", pickError);
            window->requestClose();
          }
          char title[384];
          const int minutes = static_cast<int>(clock.hour * 60.f) % (24 * 60);
          std::snprintf(title, sizeof(title),
                        "pxe  |  %02d:%02d%s  |  Tab settings   Space pause   [ ] speed   WASD / left stick move (Shift walk)   X attack (hold: charge)   Q/E / LB RB yaw   G grass fire   1/2/3 fire, water, regrow brush   4 add a fox   F fireball, IJKL roll   F4 capture   walk through the Sleeping Fox's door to go in",
                        minutes / 60, minutes % 60, clock.paused ? " (paused)" : "");
          window->setTitle(title);
          window->swap();
        }
      }
    }
  }
  return exitCode;
}
