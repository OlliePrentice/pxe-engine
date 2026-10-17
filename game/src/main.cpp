#include "scene.hpp"

#include "pxe/camera.hpp"
#include "pxe/config.hpp"
#include "pxe/gl_loader.hpp"
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
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
  int width = 1280;
  int height = 720;
  std::string screenshot;
  std::string shotsDir;
  std::string demoFrames;
  bool selfTest = false;
  bool demo = false;
  bool vsync = true;
  float yawDeg = 45.f;
  int zoom = 4;
  int weather = 0;
  float time = -1.f;
  float hour = 9.f;
  std::string cycleFrames;
  bool haveTarget = false;
  glm::vec3 target{0.f};
};

void printUsage() {
  std::fprintf(stderr,
               "pxe — 3D pixel-art prototype\n"
               "  --width N --height N\n"
               "  --screenshot path.png     save one frame and exit\n"
               "  --shots dir               save a small set of views and exit\n"
               "  --self-test               check the subpixel camera snap\n"
               "  --demo                    scripted pan and 45° rotation, then exit\n"
               "  --demo-frames dir         write that demo as a PNG sequence\n"
               "  --yaw degrees --zoom 0..4 --weather 0|1|2 --time seconds\n"
               "  --hour 0..24          time of day (morning is 9)\n"
               "  --cycle-frames dir    write one day of shadow frames, then exit\n"
               "  --target x,y,z\n"
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
    } else if (a == "--screenshot") {
      opt.screenshot = need("--screenshot");
    } else if (a == "--shots") {
      opt.shotsDir = need("--shots");
    } else if (a == "--self-test") {
      opt.selfTest = true;
    } else if (a == "--demo") {
      opt.demo = true;
      opt.vsync = false;
    } else if (a == "--demo-frames") {
      opt.demoFrames = need("--demo-frames");
      opt.vsync = false;
    } else if (a == "--no-vsync") {
      opt.vsync = false;
    } else if (a == "--yaw") {
      opt.yawDeg = std::atof(need("--yaw"));
    } else if (a == "--zoom") {
      opt.zoom = std::atoi(need("--zoom"));
    } else if (a == "--weather") {
      opt.weather = std::atoi(need("--weather"));
    } else if (a == "--time") {
      opt.time = std::atof(need("--time"));
    } else if (a == "--hour") {
      opt.hour = std::atof(need("--hour"));
    } else if (a == "--cycle-frames") {
      opt.cycleFrames = need("--cycle-frames");
      opt.vsync = false;
    } else if (a == "--target") {
      if (!parseTarget(need("--target"), opt.target)) {
        std::fprintf(stderr, "bad --target, expected x,y,z\n");
        std::exit(1);
      }
      opt.haveTarget = true;
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
  int zoom = 0;
  bool toggleOutline = false;
  bool cycleDebug = false;
  bool cycleWeather = false;
  bool shot = false;
  bool quit = false;
  bool togglePause = false;
  int timeSpeed = 0;
  int stepHour = 0;
};

bool pressed(GLFWwindow* win, int key, bool& latch) {
  const bool down = glfwGetKey(win, key) == GLFW_PRESS;
  const bool edge = down && !latch;
  latch = down;
  return edge;
}

Input pollInput(GLFWwindow* win, double& wheel) {
  Input in;
  auto down = [&](int key) { return glfwGetKey(win, key) == GLFW_PRESS; };
  if (down(GLFW_KEY_A) || down(GLFW_KEY_LEFT)) in.panX -= 1.f;
  if (down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT)) in.panX += 1.f;
  if (down(GLFW_KEY_W) || down(GLFW_KEY_UP)) in.panY += 1.f;
  if (down(GLFW_KEY_S) || down(GLFW_KEY_DOWN)) in.panY -= 1.f;
  const float speed = (down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) ? 9.f : 4.5f;
  in.panX *= speed;
  in.panY *= speed;

  static bool q = false, e = false, o = false, f1 = false, v = false, f12 = false;
  static bool minus = false, plus = false, equal = false, esc = false;
  static bool pause = false, slow = false, fast = false, stepBack = false, stepFwd = false;
  if (pressed(win, GLFW_KEY_Q, q)) in.rotate -= 1;
  if (pressed(win, GLFW_KEY_E, e)) in.rotate += 1;
  if (pressed(win, GLFW_KEY_O, o)) in.toggleOutline = true;
  if (pressed(win, GLFW_KEY_F1, f1)) in.cycleDebug = true;
  if (pressed(win, GLFW_KEY_V, v)) in.cycleWeather = true;
  if (pressed(win, GLFW_KEY_F12, f12)) in.shot = true;
  if (pressed(win, GLFW_KEY_P, pause)) in.togglePause = true;
  if (pressed(win, GLFW_KEY_LEFT_BRACKET, slow)) in.timeSpeed -= 1;
  if (pressed(win, GLFW_KEY_RIGHT_BRACKET, fast)) in.timeSpeed += 1;
  if (pressed(win, GLFW_KEY_COMMA, stepBack)) in.stepHour -= 1;
  if (pressed(win, GLFW_KEY_PERIOD, stepFwd)) in.stepHour += 1;
  if (pressed(win, GLFW_KEY_MINUS, minus) || pressed(win, GLFW_KEY_KP_SUBTRACT, minus)) in.zoom -= 1;
  if (pressed(win, GLFW_KEY_EQUAL, equal) || pressed(win, GLFW_KEY_KP_ADD, plus)) in.zoom += 1;
  if (pressed(win, GLFW_KEY_ESCAPE, esc)) in.quit = true;

  in.zoom += static_cast<int>(wheel);
  wheel = 0.0;
  return in;
}

const char* debugName(int view) {
  switch (view) {
    case 1:
      return "raw low-res";
    case 2:
      return "albedo";
    case 3:
      return "normals";
    case 4:
      return "depth";
    default:
      return "final";
  }
}

const char* weatherName(int weather) {
  switch (weather) {
    case 1:
      return "haze";
    case 2:
      return "rain";
    default:
      return "clear";
  }
}

const char* dayPhase(float hour) {
  if (hour < 5.f || hour >= 20.5f) return "night";
  if (hour < 7.2f) return "dawn";
  if (hour < 11.f) return "morning";
  if (hour < 16.2f) return "day";
  if (hour < 18.6f) return "dusk";
  return "night";
}

void wrapHour(float& hour) {
  hour = std::fmod(hour, 24.f);
  if (hour < 0.f) hour += 24.f;
}

int differingPixels(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  int diff = 0;
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; i += 4) {
    if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) ++diff;
  }
  return diff;
}

int selfTest(pxe::Renderer& renderer, pxe::Camera& camera, const pxe::Scene& scene) {
  pxe::FrameParams params;
  params.framebufferW = 640;
  params.framebufferH = 360;
  params.outlines = true;
  params.weather = 1;
  params.time = 0.4f;
  params.debugView = 0;

  auto renderLow = [&]() {
    const pxe::CameraFrame frame =
        camera.frame(pxe::RenderConfig::kRtW, pxe::RenderConfig::kRtH, pxe::RenderConfig::kViewH);
    renderer.render(scene, frame, params);
    return std::pair{frame, renderer.readLowRes()};
  };

  auto [frameA, imageA] = renderLow();
  // Step less than half a texel, and stay inside the current snap cell.
  float room = 0.49f - frameA.texelError.x;
  float deltaTexel = (room > 0.08f) ? room * 0.5f : -0.2f;
  camera.nudgeImmediate(frameA.right * frameA.texelWorld * deltaTexel);
  auto [frameB, imageB] = renderLow();
  const int subpixelDiff = differingPixels(imageA, imageB);

  camera.nudgeImmediate(frameB.right * frameB.texelWorld * 1.05f);
  auto [frameC, imageC] = renderLow();
  const int steppedDiff = differingPixels(imageA, imageC);

  const bool errorMoved = glm::distance(frameA.texelError, frameB.texelError) > 0.02f;
  const bool pass = subpixelDiff == 0 && steppedDiff > 50 && errorMoved;
  std::printf("self-test: subpixel differing pixels = %d (want 0)\n", subpixelDiff);
  std::printf("self-test: one-texel differing pixels = %d (want > 50)\n", steppedDiff);
  std::printf("self-test: snap error moved = %s  (%.3f, %.3f) -> (%.3f, %.3f)\n",
              errorMoved ? "yes" : "no", frameA.texelError.x, frameA.texelError.y,
              frameB.texelError.x, frameB.texelError.y);
  std::printf("self-test: %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  const Options opt = parseArgs(argc, argv);

  if (!glfwInit()) {
    std::fprintf(stderr, "glfwInit failed\n");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 0);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
  const bool headless = opt.selfTest || !opt.screenshot.empty() || !opt.shotsDir.empty() ||
                        !opt.demoFrames.empty() || !opt.cycleFrames.empty();
  if (headless && !opt.demo) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  GLFWwindow* window = glfwCreateWindow(opt.width, opt.height, "pxe", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "failed to create an OpenGL 4.1 window\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(opt.vsync ? 1 : 0);

  double wheel = 0.0;
  glfwSetWindowUserPointer(window, &wheel);
  glfwSetScrollCallback(window, [](GLFWwindow* w, double, double y) {
    *static_cast<double*>(glfwGetWindowUserPointer(w)) += y;
  });

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
  if (exitCode != 0) {
    // Renderer must be destroyed while the GL context is still current.
  } else {

  pxe::Scene scene = game::buildMeadow();
  renderer.uploadFoliage(scene.foliage);
  std::fprintf(stderr, "pxe: foliage instances %zu\n", scene.foliage.size());

  pxe::Camera camera;
  camera.setTarget(opt.haveTarget ? opt.target : scene.focus);
  camera.setYawRadians(opt.yawDeg * 3.14159265f / 180.f);
  camera.setPixelsPerUnitIndex(opt.zoom);

  bool outlines = true;
  int debugView = 0;
  int weather = std::clamp(opt.weather, 0, 2);
  float timeSec = opt.time >= 0.f ? opt.time : 0.f;
  const bool freezeTime = opt.time >= 0.f;
  float hour = opt.hour;
  wrapHour(hour);
  bool clockPaused = false;
  int speedIndex = 0;
  const float kSpeed[] = {1.f, 4.f, 16.f, 64.f};
  constexpr float kDaySeconds = 60.f;

  auto drawFrame = [&](int fbW, int fbH) {
    pxe::FrameParams params;
    params.framebufferW = fbW;
    params.framebufferH = fbH;
    params.outlines = outlines;
    params.debugView = debugView;
    params.weather = weather;
    params.time = timeSec;
    params.hour = hour;
    const pxe::CameraFrame frame =
        camera.frame(pxe::RenderConfig::kRtW, pxe::RenderConfig::kRtH, pxe::RenderConfig::kViewH);
    renderer.render(scene, frame, params);
    return frame;
  };

  auto saveWindow = [&](const std::string& path) {
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    drawFrame(fbW, fbH);
    const bool ok = writePng(path, fbW, fbH, renderer.readPresented(fbW, fbH));
    std::fprintf(stderr, "pxe: %s %s\n", ok ? "wrote" : "failed to write", path.c_str());
    return ok;
  };

  if (opt.selfTest) {
    exitCode = selfTest(renderer, camera, scene);
  } else if (!opt.shotsDir.empty()) {
    struct Shot {
      const char* name;
      float yaw;
      int zoom;
      int weather;
      float hour;
    };
    const Shot shots[] = {
        {"morning.png", 45.f, 4, 0, 9.f},  {"noon.png", 45.f, 4, 0, 12.5f},
        {"dawn.png", 45.f, 4, 0, 6.3f},    {"dusk.png", 45.f, 4, 0, 17.7f},
        {"night.png", 45.f, 4, 0, 22.5f},  {"morning_yaw90.png", 90.f, 4, 0, 9.f},
        {"wide.png", 45.f, 1, 0, 9.f},
    };
    bool ok = true;
    for (const Shot& shot : shots) {
      camera.setTarget(scene.focus);
      camera.setYawRadians(shot.yaw * 3.14159265f / 180.f);
      camera.setPixelsPerUnitIndex(shot.zoom);
      weather = shot.weather;
      hour = shot.hour;
      timeSec = 1.2f;
      ok = saveWindow(opt.shotsDir + "/" + shot.name) && ok;
    }
    exitCode = ok ? 0 : 1;
  } else if (!opt.cycleFrames.empty()) {
    constexpr int kFrames = 48;
    std::filesystem::create_directories(opt.cycleFrames);
    bool ok = true;
    for (int i = 0; i < kFrames; ++i) {
      hour = 24.f * static_cast<float>(i) / static_cast<float>(kFrames);
      timeSec = 1.2f;
      char name[64];
      std::snprintf(name, sizeof(name), "hour_%05.2f.png", hour);
      ok = saveWindow(opt.cycleFrames + "/" + name) && ok;
    }
    std::fprintf(stderr, "pxe: wrote %d day-cycle frames to %s\n", kFrames, opt.cycleFrames.c_str());
    exitCode = ok ? 0 : 1;
  } else if (!opt.screenshot.empty()) {
    timeSec = opt.time >= 0.f ? opt.time : 1.2f;
    exitCode = saveWindow(opt.screenshot) ? 0 : 1;
  } else if (!opt.demoFrames.empty()) {
    const float dt = 1.f / 20.f;
    float demoT = 0.f;
    bool demoRotated = false;
    int index = 0;
    std::filesystem::create_directories(opt.demoFrames);
    while (demoT < 10.2f) {
      glm::vec2 pan{0.f};
      if (demoT > 1.0f && demoT < 4.0f) pan.x = 4.0f;
      if (demoT > 4.3f && !demoRotated) {
        camera.rotateSteps(1);
        demoRotated = true;
      }
      if (demoT > 6.4f && demoT < 9.0f) pan.y = 3.4f;
      camera.setPanVelocity(pan);
      camera.update(dt);
      timeSec += dt;
      demoT += dt;
      char name[64];
      std::snprintf(name, sizeof(name), "frame_%04d.png", index++);
      if (!saveWindow(opt.demoFrames + "/" + name)) {
        exitCode = 1;
        break;
      }
    }
    std::fprintf(stderr, "pxe: wrote %d demo frames to %s\n", index, opt.demoFrames.c_str());
  } else {

  float demoT = 0.f;
  bool demoRotated = false;
  double previous = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    const double now = glfwGetTime();
    float dt = static_cast<float>(now - previous);
    previous = now;
    dt = std::clamp(dt, 0.f, 0.05f);
    if (!freezeTime) timeSec += dt;

    glfwPollEvents();
    Input input = pollInput(window, wheel);
    if (input.quit) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (input.togglePause) clockPaused = !clockPaused;
    if (input.timeSpeed) {
      speedIndex = std::clamp(speedIndex + input.timeSpeed, 0, 3);
    }
    if (input.stepHour) {
      hour += static_cast<float>(input.stepHour);
      wrapHour(hour);
    }
    if (!clockPaused && !opt.demo) {
      hour += dt * kSpeed[speedIndex] * (24.f / kDaySeconds);
      wrapHour(hour);
    }

    if (opt.demo) {
      demoT += dt;
      glm::vec2 pan{0.f};
      if (demoT > 1.0f && demoT < 4.0f) pan.x = 4.0f;
      if (demoT > 4.3f && !demoRotated) {
        camera.rotateSteps(1);
        demoRotated = true;
      }
      if (demoT > 6.4f && demoT < 9.0f) pan.y = 3.4f;
      camera.setPanVelocity(pan);
      if (demoT > 10.2f) glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else {
      camera.setPanVelocity({input.panX, input.panY});
      if (input.rotate) camera.rotateSteps(input.rotate);
      if (input.zoom) camera.zoomSteps(input.zoom);
      if (input.toggleOutline) outlines = !outlines;
      if (input.cycleDebug) debugView = (debugView + 1) % 5;
      if (input.cycleWeather) weather = (weather + 1) % 3;
      if (input.shot) {
        const std::string path = "pxe-shot.png";
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        // Drawn below; flag handled after the frame.
        (void)path;
        (void)fbW;
        (void)fbH;
      }
    }

    camera.update(dt);

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    const pxe::CameraFrame frame = drawFrame(fbW, fbH);

    if (input.shot && !opt.demo) {
      writePng("pxe-shot.png", fbW, fbH, renderer.readPresented(fbW, fbH));
      std::fprintf(stderr, "pxe: wrote pxe-shot.png\n");
    }

    const int hh = static_cast<int>(hour);
    const int mm = static_cast<int>((hour - static_cast<float>(hh)) * 60.f);
    char title[256];
    std::snprintf(title, sizeof(title),
                  "pxe  |  %02d:%02d %s %s  |  WASD pan  Q/E rotate  +/- zoom  O outlines  F1 %s  V %s  P pause  [ ] speed  , . step  |  %d px/unit",
                  hh, mm, dayPhase(hour),
                  clockPaused ? "paused" : (speedIndex == 0 ? "x1" : speedIndex == 1 ? "x4" : speedIndex == 2 ? "x16" : "x64"),
                  debugName(debugView), weatherName(weather), frame.pixelsPerUnit);
    glfwSetWindowTitle(window, title);

    glfwSwapBuffers(window);
  }
  }

  }  // init succeeded
  }  // renderer, destroyed before the context
  glfwDestroyWindow(window);
  glfwTerminate();
  return exitCode;
}
