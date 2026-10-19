#include "audio.hpp"
#include "scene.hpp"
#include "settings.hpp"

#include "pxe/camera.hpp"
#include "pxe/config.hpp"
#include "pxe/gl_loader.hpp"
#include "pxe/lighting.hpp"
#include "pxe/renderer.hpp"

#include <GLFW/glfw3.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
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
  std::string scene = "ruins";
  bool haveYaw = false;
  float yawDeg = 45.f;
  float slide = 0.f;
  float minDist = 0.34f;
  bool haveTarget = false;
  glm::vec3 target{0.f};
};

void printUsage() {
  std::fprintf(stderr,
               "pxe — contoured terrain and grass\n"
               "  --width N --height N\n"
               "  --capture path.png       640x360 low-res framebuffer, then exit\n"
               "  --window-png path.png    integer-upscaled window, then exit\n"
               "  --scene ruins|hills      default ruins\n"
               "  --hour H                 time of day, 0-24 (captures default to 15)\n"
               "  --clouds C               cloud cover 0-1 (default 0.35)\n"
               "  --rain R                 rain severity 0-1 (0.2 drizzle, 0.55 rain, 1 downpour)\n"
               "  --no-fire                campfire unlit\n"
               "  --fire-level L,S         captures: campfires burning at L (0 out .. 1), smoke S once out\n"
               "  --shelter-test           ruins: add a fire under a stone shelter and one under a tree\n"
               "  --extra-fires N          ruins: N bare flames over the lawn, to test many fires\n"
               "  --candles                ruins: candles on the stonework and the lawn (and under the --shelter-test roof)\n"
               "  --settle S               captures: run the weather S seconds first (rain puts fires out)\n"
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
               "  --min-dist metres        clump Poisson spacing (default 0.34)\n"
               "  --time seconds           wind time for captures (default 0)\n"
               "  --hide-grass --magenta --ndl --test-colours --no-tufts --no-edges\n"
               "  --stamps --no-depth\n"
               "  --self-test              snap stability of the low-res image\n"
               "  --no-vsync\n");
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

bool writePng(const std::string& path, int w, int h, const std::vector<std::uint8_t>& bottomUp) {
  if (w <= 0 || h <= 0 || bottomUp.size() < static_cast<std::size_t>(w * h * 4)) return false;
  std::filesystem::path file(path);
  if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path());
  std::vector<std::uint8_t> top(static_cast<std::size_t>(w * h * 4));
  const int stride = w * 4;
  for (int y = 0; y < h; ++y) {
    std::memcpy(top.data() + static_cast<std::size_t>(h - 1 - y) * stride,
                bottomUp.data() + static_cast<std::size_t>(y) * stride, stride);
  }
  return stbi_write_png(path.c_str(), w, h, 4, top.data(), stride) != 0;
}

struct Input {
  float panX = 0.f;
  float panY = 0.f;
  int rotate = 0;
  bool toggleGrass = false;
  bool toggleMagenta = false;
  bool toggleNdl = false;
  bool toggleMenu = false;
  bool cycleRain = false;
  bool strike = false;
  bool toggleSound = false;
  bool togglePause = false;
  int speedSteps = 0;  // +1 doubles, -1 halves
  bool shot = false;
  bool quit = false;
};

bool pressed(GLFWwindow* win, int key, bool& latch) {
  const bool down = glfwGetKey(win, key) == GLFW_PRESS;
  const bool edge = down && !latch;
  latch = down;
  return edge;
}

Input pollInput(GLFWwindow* win, bool keyboardTaken) {
  Input in;
  static bool tab = false;
  if (pressed(win, GLFW_KEY_TAB, tab)) in.toggleMenu = true;
  if (keyboardTaken) return in;
  auto down = [&](int key) { return glfwGetKey(win, key) == GLFW_PRESS; };
  if (down(GLFW_KEY_A) || down(GLFW_KEY_LEFT)) in.panX -= 1.f;
  if (down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT)) in.panX += 1.f;
  if (down(GLFW_KEY_W) || down(GLFW_KEY_UP)) in.panY += 1.f;
  if (down(GLFW_KEY_S) || down(GLFW_KEY_DOWN)) in.panY -= 1.f;
  const float speed = (down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) ? 14.f : 6.f;
  in.panX *= speed;
  in.panY *= speed;

  static bool q = false, e = false, f1 = false, f2 = false, f3 = false, f4 = false, esc = false, space = false,
              slower = false, faster = false;
  if (pressed(win, GLFW_KEY_SPACE, space)) in.togglePause = true;
  static bool rkey = false;
  if (pressed(win, GLFW_KEY_R, rkey)) in.cycleRain = true;
  static bool lkey = false;
  if (pressed(win, GLFW_KEY_L, lkey)) in.strike = true;
  static bool mkey = false;
  if (pressed(win, GLFW_KEY_M, mkey)) in.toggleSound = true;
  if (pressed(win, GLFW_KEY_LEFT_BRACKET, slower)) in.speedSteps -= 1;
  if (pressed(win, GLFW_KEY_RIGHT_BRACKET, faster)) in.speedSteps += 1;
  if (pressed(win, GLFW_KEY_Q, q)) in.rotate -= 1;
  if (pressed(win, GLFW_KEY_E, e)) in.rotate += 1;
  if (pressed(win, GLFW_KEY_F1, f1)) in.toggleNdl = true;
  if (pressed(win, GLFW_KEY_F2, f2)) in.toggleGrass = true;
  if (pressed(win, GLFW_KEY_F3, f3)) in.toggleMagenta = true;
  if (pressed(win, GLFW_KEY_F4, f4)) in.shot = true;
  if (pressed(win, GLFW_KEY_ESCAPE, esc)) in.quit = true;
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

pxe::Lighting lightingFor(const pxe::Scene& scene, const game::WorldClock& clock) {
  pxe::Lighting lighting = pxe::lightingAt(scene, clock.hour);
  lighting.cloudCover = clock.cloudCover;
  lighting.cloudOffset = clock.cloudOffset;
  lighting.cloudStrength = clock.cloudStrength;
  lighting.windDir = clock.windDir();
  lighting.windStrength = clock.cloudSpeed;
  // Rain brings a murky, even overcast that fades sun and moon shadows. The
  // drifting cloud shadows fade out as it sets in - under a rain sky there
  // are no separate clouds to see - and are gone from moderate rain up.
  lighting.rain = clock.rain;
  lighting.overcast = 0.7f * clock.rain;
  lighting.cloudStrength *= std::clamp(1.f - 2.f * clock.rain, 0.f, 1.f);

  // God rays: only from the sun. Strong in the morning and at dusk while it
  // is low, barely there with it high. A drizzle catches the light and
  // brings them out more; from moderate rain up the sun is gone and so are
  // they. Cloud shadows block them where they fall (in the edge pass).
  {
    const float elev = lighting.sunElevationDegrees;
    const bool sunLights = pxe::sunDirection(scene, clock.hour).y >= pxe::moonDirection(scene, clock.hour).y;
    auto ramp = [](float x, float a, float b) {
      const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
      return t * t * (3.f - 2.f * t);
    };
    const float low = 1.f - ramp(elev, 8.f, 35.f);
    const float r = clock.rain;
    const float rainLight = r <= 0.3f ? 1.f + 2.5f * r : std::max(0.f, 1.75f * (1.f - (r - 0.3f) / 0.15f));
    lighting.rays = sunLights ? ramp(elev, 0.f, 3.f) * (0.1f + low) * rainLight : 0.f;
    lighting.rayColour = glm::mix(glm::vec3{1.f, 0.97f, 0.86f}, glm::vec3{1.f, 0.8f, 0.56f}, 1.f - ramp(elev, 6.f, 24.f));
  }

  // Firefly light, like the firelight: direct sun drowns it out, so by day
  // it shows only in the shade, and at night it lights all round them.
  {
    const float dark = std::clamp((8.f - lighting.sunElevationDegrees) / 16.f, 0.f, 1.f);
    lighting.fireflyLight = 1.4f;
    lighting.fireflyDim = 0.95f * (1.f - dark);
    const pxe::Lighting noon = pxe::lightingAt(scene, 12.f);
    for (int s = 0; s < pxe::kSurfCount; ++s)
      lighting.fireflyBand[s] = glm::mix(noon.palette[s].band[2], glm::vec3{0.85f, 1.f, 0.5f}, 0.5f);
  }

  // Firelight: a warm pool round the campfire. By day direct sun drowns it
  // out, so it shows only in the shade round the fire; once the sun is down
  // it lights everything near and carries farther. Each surface's colour in
  // full firelight is its midday lit colour pulled toward a golden orange.
  {
    const float dark = std::clamp((8.f - lighting.sunElevationDegrees) / 16.f, 0.f, 1.f);
    lighting.fireLight = 1.5f;
    lighting.fireDim = 0.9f * (1.f - dark);
    lighting.fireReach = 6.f + 3.5f * dark;
    const pxe::Lighting noon = pxe::lightingAt(scene, 12.f);
    for (int s = 0; s < pxe::kSurfCount; ++s)
      lighting.fireBand[s] = glm::mix(noon.palette[s].band[2] * 1.05f, glm::vec3{1.f, 0.52f, 0.34f}, 0.55f);
  }

  // A lightning flash: a second light from the flash's direction with hard
  // shadows, blended over the sun or moon by its strength, and the overcast
  // drops away while it lasts. Lit faces take the midday colours washed out
  // to a cold blue-white; the shadow side stays close to the storm's dark.
  if (clock.flash > 0.f) {
    const float f = clock.flash;
    const pxe::Lighting day = pxe::lightingAt(scene, 12.f);
    auto cold = [](glm::vec3 c) {
      const float l = glm::dot(c, glm::vec3{0.299f, 0.587f, 0.114f});
      return glm::mix(c, glm::vec3{l}, 0.55f) * glm::vec3{0.86f, 0.95f, 1.18f};
    };
    for (int s = 0; s < pxe::kSurfCount; ++s)
      for (int b = 0; b < 3; ++b) {
        pxe::Palette& p = lighting.palette[s];
        const pxe::Palette& d = day.palette[s];
        const float k = f * (b == 0 ? 0.35f : 1.f);
        p.band[b] = glm::mix(p.band[b], cold(d.band[b]), k);
        p.detail[b] = glm::mix(p.detail[b], cold(d.detail[b]), k);
        p.tip[b] = glm::mix(p.tip[b], cold(d.tip[b]), k);
      }
    lighting.rim = glm::mix(lighting.rim, cold(day.rim), f);
    lighting.ink = glm::mix(lighting.ink, day.ink, f);
    lighting.flashDir = clock.flashDir;
    lighting.overcast *= 1.f - f;
    lighting.flash = f;
  }
  return lighting;
}

// The fires burning this frame, for the renderer.
std::vector<pxe::FireDraw> fireDraws(const game::WorldClock& clock) {
  std::vector<pxe::FireDraw> draws;
  for (const game::WorldClock::Fire& f : clock.fires) draws.push_back({f.id, f.base, f.level, f.smoke, f.size});
  return draws;
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
  params.present = true;
  return params;
}

int selfTest(pxe::Renderer& renderer, pxe::Camera& camera, const pxe::Scene& scene) {
  pxe::FrameParams params;
  params.lighting = pxe::lightingAt(scene, kCaptureHour);
  for (std::uint32_t i = 0; i < scene.fires.size(); ++i)
    params.fires.push_back({i, scene.fires[i].base, 1.f, 0.f, scene.fires[i].size});
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
  std::vector<std::int16_t> pcm;
  for (int sec = 0; sec < kSeconds; ++sec) {
    if (sec == 2) audio.thunderAt(-0.7f, 0.05f);
    if (sec == 10) audio.thunderAt(0.7f, 0.8f);
    audio.render(buf.data(), rate);
    double sum = 0.0;
    float peak = 0.f;
    for (float v : buf) {
      sum += static_cast<double>(v) * v;
      peak = std::max(peak, std::fabs(v));
      pcm.push_back(static_cast<std::int16_t>(std::clamp(v, -1.f, 1.f) * 32767.f));
    }
    std::fprintf(stderr, "audio: %2d s  rms %.3f  peak %.3f\n", sec, std::sqrt(sum / buf.size()), peak);
  }
  std::FILE* f = std::fopen(opt.audioWav.c_str(), "wb");
  if (!f) return 1;
  auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
  const std::uint32_t bytes = static_cast<std::uint32_t>(pcm.size() * 2);
  std::fwrite("RIFF", 1, 4, f);
  u32(36 + bytes);
  std::fwrite("WAVEfmt ", 1, 8, f);
  u32(16);
  u16(1);
  u16(2);
  u32(static_cast<std::uint32_t>(rate));
  u32(static_cast<std::uint32_t>(rate) * 4);
  u16(4);
  u16(16);
  std::fwrite("data", 1, 4, f);
  u32(bytes);
  std::fwrite(pcm.data(), 2, pcm.size(), f);
  std::fclose(f);
  std::fprintf(stderr, "pxe: wrote %s\n", opt.audioWav.c_str());
  return 0;
}

int main(int argc, char** argv) {
  const Options opt = parseArgs(argc, argv);
  if (!opt.audioWav.empty()) return writeAudioWav(opt);

  if (!glfwInit()) {
    std::fprintf(stderr, "glfwInit failed\n");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 0);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
  const bool headless = opt.selfTest || !opt.capture.empty() || !opt.windowPng.empty() || opt.bench > 0;
  if (headless || !opt.uiShot.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  GLFWwindow* window = glfwCreateWindow(opt.width, opt.height, "pxe", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "failed to create an OpenGL 4.1 window\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(opt.vsync && !headless ? 1 : 0);
  pxe::loadGl(+[](const char* name) -> void* { return reinterpret_cast<void*>(glfwGetProcAddress(name)); });

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
      // Grass and leaf sprites are sized in pixels and their spacing was tuned
      // at 16 px/m, so scale it to keep the same density per pixel.
      const float density = pxe::RenderConfig::kPixelsPerUnit / 16.f;
      build.minDist = opt.minDist / density;
      build.tuftDist /= density;
      build.leafDensity *= density * density;
      build.tufts = opt.tufts && !opt.stamps;
      build.stamps = opt.stamps;
      build.shelterTest = opt.shelterTest;
      build.extraFires = opt.extraFires;
      build.candles = opt.candles;
      build.scene = opt.scene;
      pxe::Scene scene;
      try {
        scene = game::buildScene(build);
      } catch (const std::exception& ex) {
        std::fprintf(stderr, "scene build failed: %s\n", ex.what());
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
      }
      renderer.upload(scene);

      pxe::Camera camera;
      camera.setYawRadians((opt.haveYaw ? opt.yawDeg : scene.yawDegrees) * 3.14159265f / 180.f);
      camera.setTarget(opt.haveTarget ? opt.target : scene.focus);
      if (opt.slide != 0.f) {
        const float yaw = camera.yawRadians();
        const glm::vec3 flatForward{std::sin(yaw), 0.f, std::cos(yaw)};
        camera.setTarget(camera.target() + flatForward * opt.slide);
      }

      auto draw = [&](const pxe::FrameParams& params) {
        const pxe::CameraFrame frame = camera.frame(renderer.rtW(), renderer.rtH());
        renderer.render(scene, frame, params);
        return frame;
      };

      if (opt.bench > 0) {
        // Same frame as the default window, wind running, timed per pass.
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        game::WorldClock clock;
        clock.hour = opt.hour >= 0.f ? opt.hour : kCaptureHour;
        clock.cloudCover = opt.clouds;
        clock.rain = opt.rain;
        clock.lightning = opt.lightning;
        clock.windDegrees = opt.windDegrees;
        clock.cloudSpeed = opt.windStrength;
        clock.placeFires(scene.fires);
        // Per-pass GPU timers block every frame and, on macOS's GL-on-Metal,
        // under-report; they are opt-in. The wall-clock figure is the one to trust.
        const bool passes = std::getenv("PXE_BENCH_PASSES") != nullptr;
        renderer.setProfiling(passes);
        double sum[pxe::PassTimes::kCount] = {};
        const int warmup = 10;
        const double start = glfwGetTime();
        double timedStart = start;
        for (int f = 0; f < warmup + opt.bench; ++f) {
          if (f == warmup) timedStart = glfwGetTime();
          pxe::FrameParams params = makeParams(opt);
          params.framebufferW = fbW;
          params.framebufferH = fbH;
          params.time = static_cast<float>(f) / 60.f;
          // Advance the day as the game does at default speed, so the sun
          // moves and the shadow map is redrawn every frame (unless paused).
          if (!opt.benchPaused) clock.advance(1.f / 60.f);
          params.fires = fireDraws(clock);
          params.lighting = lightingFor(scene, clock);
          draw(params);
          glfwSwapBuffers(window);
          if (f >= warmup)
            for (int i = 0; i < pxe::PassTimes::kCount; ++i) sum[i] += renderer.passTimes().ms[i];
        }
        const double wall = (glfwGetTime() - timedStart) / opt.bench * 1000.0;
        if (passes) {
          for (int i = 0; i < pxe::PassTimes::kCount; ++i)
            std::fprintf(stderr, "bench: %-10s %6.2f ms (GPU timer)\n", pxe::PassTimes::name(i), sum[i] / opt.bench);
        }
        std::fprintf(stderr, "bench: wall %6.2f ms/frame (%.0f fps uncapped), %dx%d\n", wall, 1000.0 / wall, fbW, fbH);
      } else if (opt.selfTest) {
        exitCode = selfTest(renderer, camera, scene);
      } else if (headless) {
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        game::WorldClock clock;
        clock.hour = opt.hour >= 0.f ? opt.hour : kCaptureHour;
        clock.cloudCover = opt.clouds;
        clock.rain = opt.rain;
        clock.lightning = opt.lightning;
        clock.placeFires(scene.fires);
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
            clock.advance(1.f / 60.f);
            pxe::FrameParams params = makeParams(opt);
            params.framebufferW = fbW;
            params.framebufferH = fbH;
            params.time = opt.time + static_cast<float>(f) / 60.f;
            params.fires = fireDraws(clock);
            params.lighting = lightingFor(scene, clock);
            draw(params);
          }
          clock.paused = false;
          clock.lightning = lightning;
          for (const game::WorldClock::Fire& f : clock.fires)
            std::fprintf(stderr, "pxe: fire %s, level %.2f, smoke %.2f\n", f.lit ? "lit" : "out", f.level, f.smoke);
        }
        if (opt.flash > 0.f) {
          // Compass bearing, like the wind: 0 north (-z), 90 east (+x).
          const float az = opt.flashAz * 3.14159265f / 180.f, el = opt.flashEl * 3.14159265f / 180.f;
          clock.flash = opt.flash;
          clock.flashDir = {std::cos(el) * std::sin(az), std::sin(el), -std::cos(el) * std::cos(az)};
        }
        clock.windDegrees = opt.windDegrees;
        clock.cloudSpeed = opt.windStrength;
        pxe::FrameParams params = makeParams(opt);
        params.framebufferW = fbW;
        params.framebufferH = fbH;
        params.fires = fireDraws(clock);
        if (opt.settle > 0.f) params.time = opt.time + std::ceil(opt.settle * 60.f) / 60.f;
        params.lighting = lightingFor(scene, clock);
        const pxe::CameraFrame frame = draw(params);
        std::fprintf(stderr, "pxe: snap error %.4f %.4f\n", frame.texelError.x, frame.texelError.y);
        bool ok = true;
        if (!opt.capture.empty()) {
          ok = writePng(opt.capture, renderer.viewW(), renderer.viewH(),
                        renderer.readView()) &&
               ok;
          std::fprintf(stderr, "pxe: %s %s\n", ok ? "wrote" : "failed", opt.capture.c_str());
        }
        if (!opt.windowPng.empty()) {
          int fbW = 0, fbH = 0;
          glfwGetFramebufferSize(window, &fbW, &fbH);
          const bool wrote = writePng(opt.windowPng, fbW, fbH, renderer.readPresented(fbW, fbH));
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
        clock.placeFires(scene.fires);
        game::SoundSettings sound;
        sound.on = opt.sound;
        game::Audio audio;
        if (!audio.start()) std::fprintf(stderr, "pxe: no audio output, running silent\n");
        int heardStrikes = 0;
        game::SettingsMenu menu;
        menu.init(window);
        if (opt.menu) menu.toggle();
        // Averaged over a whole second, so a slow first frame or a hitch
        // does not skew it; with vsync on it tops out at the display rate.
        float fps = 0.f;
        float fpsTime = 0.f;
        int fpsFrames = 0;
        int frameCount = 0;
        double previous = glfwGetTime();
        while (!glfwWindowShouldClose(window)) {
          const double now = glfwGetTime();
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
          glfwPollEvents();
          Input input = pollInput(window, menu.wantsKeyboard());
          if (input.quit) glfwSetWindowShouldClose(window, GLFW_TRUE);
          if (input.toggleMenu) menu.toggle();
          if (input.togglePause) clock.paused = !clock.paused;
          if (input.strike) clock.strike();
          if (input.toggleSound) sound.on = !sound.on;
          if (input.cycleRain) clock.rain = clock.rain <= 0.f ? 0.2f : clock.rain < 0.5f ? 0.55f : clock.rain < 0.9f ? 1.f : 0.f;
          if (input.speedSteps)
            clock.hoursPerMinute = std::clamp(clock.hoursPerMinute * std::pow(2.f, static_cast<float>(input.speedSteps)),
                                              1.f / 60.f, 120.f);
          clock.advance(dt);
          if (input.toggleGrass) view.grass = !view.grass;
          if (input.toggleMagenta) magenta = !magenta;
          if (input.toggleNdl) ndl = !ndl;
          camera.setPanVelocity({input.panX, input.panY});
          if (input.rotate) camera.rotateSteps(input.rotate);
          camera.update(dt);

          int fbW = 0, fbH = 0;
          glfwGetFramebufferSize(window, &fbW, &fbH);
          pxe::FrameParams params = makeParams(opt);
          params.framebufferW = fbW;
          params.framebufferH = fbH;
          params.showGrass = view.grass;
          params.edges = view.edges;
          params.fire = view.fire;
          params.rays = view.rays;
          params.fireflies = view.fireflies;
          params.fires = fireDraws(clock);
          params.magentaGrass = magenta;
          params.showNdl = ndl;
          params.time = static_cast<float>(now);
          params.lighting = lightingFor(scene, clock);
          const pxe::CameraFrame frame = draw(params);
          // Weather sound follows the settings; each new flash queues its
          // thunder, panned toward the side of the screen it lit from.
          audio.setVolumes(sound);
          audio.setWeather(clock.cloudSpeed, clock.rain);
          if (clock.strikes != heardStrikes) {
            heardStrikes = clock.strikes;
            audio.thunder(glm::dot(clock.flashDir, frame.right) / std::max(glm::length(glm::vec2{clock.flashDir.x, clock.flashDir.z}), 1e-3f));
          }
          if (input.shot) {
            const bool ok = writePng("shot.png", renderer.viewW(), renderer.viewH(),
                                     renderer.readView());
            std::fprintf(stderr, "pxe: %s shot.png\n", ok ? "wrote" : "failed");
          }
          menu.draw(clock, view, sound, fps);
          if (!opt.uiShot.empty() && ++frameCount == 8) {
            const bool ok = writePng(opt.uiShot, fbW, fbH, renderer.readPresented(fbW, fbH));
            std::fprintf(stderr, "pxe: %s %s (%dx%d)\n", ok ? "wrote" : "failed", opt.uiShot.c_str(), fbW, fbH);
            glfwSetWindowShouldClose(window, GLFW_TRUE);
          }
          char title[200];
          const int minutes = static_cast<int>(clock.hour * 60.f) % (24 * 60);
          std::snprintf(title, sizeof(title),
                        "pxe  |  %02d:%02d%s  |  Tab settings   Space pause   [ ] speed   WASD pan   Q/E yaw   F4 capture",
                        minutes / 60, minutes % 60, clock.paused ? " (paused)" : "");
          glfwSetWindowTitle(window, title);
          (void)frame;
          glfwSwapBuffers(window);
        }
        menu.shutdown();
      }
    }
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return exitCode;
}
