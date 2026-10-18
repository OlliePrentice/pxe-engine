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
  float time = 0.f;
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
               "  --yaw degrees            default 45\n"
               "  --slide metres           move the focus along horizontal forward\n"
               "  --target x,y,z\n"
               "  --min-dist metres        clump Poisson spacing (default 0.34)\n"
               "  --time seconds           wind time for captures (default 0)\n"
               "  --hide-grass --magenta --ndl --test-colours --no-tufts\n"
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
  bool shot = false;
  bool quit = false;
};

bool pressed(GLFWwindow* win, int key, bool& latch) {
  const bool down = glfwGetKey(win, key) == GLFW_PRESS;
  const bool edge = down && !latch;
  latch = down;
  return edge;
}

Input pollInput(GLFWwindow* win) {
  Input in;
  auto down = [&](int key) { return glfwGetKey(win, key) == GLFW_PRESS; };
  if (down(GLFW_KEY_A) || down(GLFW_KEY_LEFT)) in.panX -= 1.f;
  if (down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT)) in.panX += 1.f;
  if (down(GLFW_KEY_W) || down(GLFW_KEY_UP)) in.panY += 1.f;
  if (down(GLFW_KEY_S) || down(GLFW_KEY_DOWN)) in.panY -= 1.f;
  const float speed = (down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) ? 14.f : 6.f;
  in.panX *= speed;
  in.panY *= speed;

  static bool q = false, e = false, f1 = false, f2 = false, f3 = false, f4 = false, esc = false;
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
  params.present = true;
  return params;
}

int selfTest(pxe::Renderer& renderer, pxe::Camera& camera, const pxe::Scene& scene) {
  pxe::FrameParams params;
  params.framebufferW = 320;
  params.framebufferH = 180;
  params.present = false;

  auto grab = [&]() {
    const pxe::CameraFrame frame = camera.frame(pxe::RenderConfig::kRtW, pxe::RenderConfig::kRtH);
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
  const int panDiff = shiftedDiff(imageA, imageC, pxe::RenderConfig::kViewW, pxe::RenderConfig::kViewH, 1);

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
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
  const bool headless = opt.selfTest || !opt.capture.empty() || !opt.windowPng.empty();
  if (headless) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

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
      build.minDist = opt.minDist;
      build.tufts = opt.tufts && !opt.stamps;
      build.stamps = opt.stamps;
      pxe::Scene scene = game::buildTerrain(build);
      renderer.upload(scene);

      pxe::Camera camera;
      camera.setYawRadians(opt.yawDeg * 3.14159265f / 180.f);
      if (opt.haveTarget) camera.setTarget(opt.target);
      if (opt.slide != 0.f) {
        const float yaw = camera.yawRadians();
        const glm::vec3 flatForward{std::sin(yaw), 0.f, std::cos(yaw)};
        camera.setTarget(camera.target() + flatForward * opt.slide);
      }

      auto draw = [&](const pxe::FrameParams& params) {
        const pxe::CameraFrame frame = camera.frame(pxe::RenderConfig::kRtW, pxe::RenderConfig::kRtH);
        renderer.render(scene, frame, params);
        return frame;
      };

      if (opt.selfTest) {
        exitCode = selfTest(renderer, camera, scene);
      } else if (headless) {
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        pxe::FrameParams params = makeParams(opt);
        params.framebufferW = fbW;
        params.framebufferH = fbH;
        const pxe::CameraFrame frame = draw(params);
        std::fprintf(stderr, "pxe: snap error %.4f %.4f\n", frame.texelError.x, frame.texelError.y);
        bool ok = true;
        if (!opt.capture.empty()) {
          ok = writePng(opt.capture, pxe::RenderConfig::kViewW, pxe::RenderConfig::kViewH,
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
        bool showGrass = !opt.hideGrass;
        bool magenta = opt.magenta;
        bool ndl = opt.ndl;
        double previous = glfwGetTime();
        while (!glfwWindowShouldClose(window)) {
          const double now = glfwGetTime();
          float dt = static_cast<float>(now - previous);
          previous = now;
          dt = std::clamp(dt, 0.f, 0.05f);
          glfwPollEvents();
          Input input = pollInput(window);
          if (input.quit) glfwSetWindowShouldClose(window, GLFW_TRUE);
          if (input.toggleGrass) showGrass = !showGrass;
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
          params.showGrass = showGrass;
          params.magentaGrass = magenta;
          params.showNdl = ndl;
          params.time = static_cast<float>(now);
          const pxe::CameraFrame frame = draw(params);
          if (input.shot) {
            const bool ok = writePng("shot.png", pxe::RenderConfig::kViewW, pxe::RenderConfig::kViewH,
                                     renderer.readView());
            std::fprintf(stderr, "pxe: %s shot.png\n", ok ? "wrote" : "failed");
          }
          char title[160];
          std::snprintf(title, sizeof(title),
                        "pxe  |  WASD pan   Q/E yaw   F1 N·L %s   F2 grass %s   F3 magenta %s   F4 capture",
                        ndl ? "on" : "off", showGrass ? "on" : "off", magenta ? "on" : "off");
          glfwSetWindowTitle(window, title);
          (void)frame;
          glfwSwapBuffers(window);
        }
      }
    }
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return exitCode;
}
