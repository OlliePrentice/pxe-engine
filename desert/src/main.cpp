// pxe_desert: a first-person walk over sand dunes to a date palm, in the
// engine's pixel-art style. The view is from the eye at real scale, with the
// player's arms, no tools or HUD.
//
// Keyboard and mouse: the mouse looks, WASD walks, Shift sprints. The cursor
// is held while playing; Escape lets it go, Escape again quits, and a click
// in the window takes it back. Gamepad: the left stick walks, the right
// stick looks, and the left stick pressed in sprints while held.

#include "arms.hpp"
#include "player.hpp"
#include "world.hpp"

#include "pxe/config.hpp"
#include "pxe/first_person_camera.hpp"
#include "pxe/image.hpp"
#include "pxe/input.hpp"
#include "pxe/lighting.hpp"
#include "pxe/renderer.hpp"
#include "pxe/window.hpp"

#include <glm/vec2.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>

namespace {

constexpr float kHour = 16.5f;  // the fixed time of day, 0-24, unless --hour says
constexpr int kCaptureFrames = 4;
// The spawn, and the --capture pose: 25 m south of the palm (toward -z,
// Scene::sunSouth), facing it, looking level.
constexpr glm::vec2 kSpawn{0.f, -25.f};
// A stall (a dragged window) moves the player at most this many seconds' worth.
constexpr float kMaxStep = 0.05f;
// Looking: radians per window point of mouse motion, and radians a second
// with a stick held fully over.
constexpr float kLookPerPoint = 0.0015f;
constexpr float kStickTurnRate = 2.4f;  // left and right
constexpr float kStickTiltRate = 1.6f;  // up and down

struct Options {
  std::string capture;
  std::string windowPng;
  float hour = kHour;
};

void printUsage(std::FILE* out) {
  std::fprintf(out,
               "pxe_desert — first person on the sand dunes\n"
               "  --capture path.png       the %dx%d low-res view from the spawn, then exit\n"
               "  --window-png path.png    the integer-upscaled window from the spawn, then exit\n"
               "  --hour H                 time of day, 0-24 (default %.4g); the clock doesn't run\n"
               "  --help                   this list\n"
               "Play: the mouse looks, WASD walks, Shift sprints; Escape frees the cursor, a click takes\n"
               "it back, Escape with it free quits. Gamepad: left stick walks (pressed in: sprint),\n"
               "right stick looks.\n",
               pxe::RenderConfig::kViewW, pxe::RenderConfig::kViewH, static_cast<double>(kHour));
}

// Reads the command line into `opt`. Returns -1 to go on, or the exit code.
int parseArgs(int argc, char** argv, Options& opt) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    bool ok = true;
    if (arg == "--help" || arg == "-h") {
      printUsage(stdout);
      return 0;
    } else if (arg == "--capture" && i + 1 < argc) {
      opt.capture = argv[++i];
    } else if (arg == "--window-png" && i + 1 < argc) {
      opt.windowPng = argv[++i];
    } else if (arg == "--hour" && i + 1 < argc) {
      char* end = nullptr;
      opt.hour = std::strtof(argv[++i], &end);
      ok = *end == '\0' && opt.hour >= 0.f && opt.hour <= 24.f;
    } else {
      ok = false;
    }
    if (!ok) {
      std::fprintf(stderr, "pxe_desert: bad argument: %s\n", arg.c_str());
      printUsage(stderr);
      return 1;
    }
  }
  return -1;
}

// The game's input actions: what the keys, mouse and pad ask the player to
// do this frame, over `dt` seconds.
desert::PlayerInput readInput(const pxe::Input& input, float dt) {
  desert::PlayerInput in;
  glm::vec2 keys{0.f};
  if (input.keyDown(pxe::Key::kW)) keys.y += 1.f;
  if (input.keyDown(pxe::Key::kS)) keys.y -= 1.f;
  if (input.keyDown(pxe::Key::kD)) keys.x += 1.f;
  if (input.keyDown(pxe::Key::kA)) keys.x -= 1.f;
  if (keys.x != 0.f || keys.y != 0.f) keys /= std::hypot(keys.x, keys.y);
  // The stick's y points down: pushed up, it walks forward.
  const glm::vec2 stick = input.gamepadStick(pxe::GamepadStick::kLeft);
  in.move = keys + glm::vec2{stick.x, -stick.y};
  in.sprint = input.keyDown(pxe::Key::kLeftShift) || input.keyDown(pxe::Key::kRightShift) ||
              input.gamepadDown(pxe::GamepadButton::kLeftStick);
  // Mouse right turns right and down looks down; the right stick the same
  // way, pushed up looking up.
  const glm::vec2 look = input.gamepadStick(pxe::GamepadStick::kRight);
  in.look = input.mouseDelta() * kLookPerPoint + glm::vec2{look.x * kStickTurnRate, look.y * kStickTiltRate} * dt;
  return in;
}

// One frame's draw list, built fresh: the scene lit at the fixed hour under
// a clear, still sky. The player's arms are added on top (Arms::pose).
pxe::FrameParams frameParams(const desert::World& world, glm::ivec2 framebuffer, float time, float hour) {
  pxe::FrameParams params;
  params.framebufferW = framebuffer.x;
  params.framebufferH = framebuffer.y;
  params.time = time;
  params.lighting = pxe::lightingAt(world.scene, hour, pxe::Weather{});
  return params;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  if (const int exitCode = parseArgs(argc, argv, opt); exitCode >= 0) return exitCode;
  const bool headless = !opt.capture.empty() || !opt.windowPng.empty();

  try {
    // The window first: it makes the GL context everything after it needs.
    pxe::WindowOptions options;
    options.width = 1920;
    options.height = 1080;
    options.title = "pxe desert";
    options.visible = !headless;
    options.vsync = !headless;
    pxe::Window window(options);

    pxe::Renderer renderer;
    renderer.init(pxe::RenderMode::kPerspective);

    desert::World world = desert::buildWorld();
    renderer.upload(world.scene);

    desert::Arms arms;
    arms.load();

    // At the engine's default 60 degree field of view and 300 m far plane.
    pxe::FirstPersonCamera camera;
    desert::Player player;
    player.spawn(kSpawn, std::atan2(desert::kPalmSpot.x - kSpawn.x, desert::kPalmSpot.y - kSpawn.y), 0.f);
    player.aim(camera);

    if (headless) {
      // A few frames at fixed times, so a capture is the same on every run.
      for (int f = 0; f < kCaptureFrames; ++f) {
        pxe::FrameParams params =
            frameParams(world, window.framebufferSize(), static_cast<float>(f) / 60.f, opt.hour);
        const pxe::PerspectiveFrame view = camera.frame(renderer.rtW(), renderer.rtH());
        arms.pose(view, params);
        renderer.render(world.scene, view, params);
      }
      bool ok = true;
      if (!opt.capture.empty()) {
        const bool wrote = pxe::writePng(opt.capture, renderer.viewW(), renderer.viewH(), renderer.readView());
        std::fprintf(stderr, "pxe_desert: %s %s\n", wrote ? "wrote" : "failed to write", opt.capture.c_str());
        ok = wrote && ok;
      }
      if (!opt.windowPng.empty()) {
        const glm::ivec2 fb = window.framebufferSize();
        const bool wrote = pxe::writePng(opt.windowPng, fb.x, fb.y, renderer.readPresented(fb.x, fb.y));
        std::fprintf(stderr, "pxe_desert: %s %s (%dx%d)\n", wrote ? "wrote" : "failed to write",
                     opt.windowPng.c_str(), fb.x, fb.y);
        ok = wrote && ok;
      }
      return ok ? 0 : 1;
    }

    pxe::Input input;
    // Mouse look holds the cursor in the window.
    window.setCursorCaptured(true);
    double previous = window.time();
    while (!window.shouldClose()) {
      const double now = window.time();
      const float dt = std::min(static_cast<float>(now - previous), kMaxStep);
      previous = now;

      window.pollEvents();
      input.update(window);
      // Escape lets the cursor go, and quits once it is free; a click takes
      // it back. The player looks and walks only while it is held.
      if (input.keyPressed(pxe::Key::kEscape)) {
        if (window.cursorCaptured()) {
          window.setCursorCaptured(false);
        } else {
          window.requestClose();
        }
      } else if (!window.cursorCaptured() && input.mousePressed(pxe::MouseButton::kLeft)) {
        window.setCursorCaptured(true);
      }
      if (window.cursorCaptured()) {
        player.update(readInput(input, dt), dt);
        player.aim(camera);
      }

      // A minimised window can have a 0x0 framebuffer, and render throws when
      // asked to present at that size: skip drawing, and wait rather than spin.
      const glm::ivec2 framebuffer = window.framebufferSize();
      if (framebuffer.x <= 0 || framebuffer.y <= 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        continue;
      }
      pxe::FrameParams params = frameParams(world, framebuffer, static_cast<float>(now), opt.hour);
      const pxe::PerspectiveFrame view = camera.frame(renderer.rtW(), renderer.rtH());
      arms.pose(view, params);
      renderer.render(world.scene, view, params);
      window.swap();
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "pxe_desert: %s\n", e.what());
    return 1;
  }
  return 0;
}
