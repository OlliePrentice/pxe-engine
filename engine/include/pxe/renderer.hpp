#pragma once

#include "pxe/camera.hpp"
#include "pxe/scene.hpp"

#include <cstdint>
#include <vector>

namespace pxe {

struct FrameParams {
  int framebufferW = 1920;
  int framebufferH = 1080;
  bool showGrass = true;
  bool magentaGrass = false;
  bool showNdl = false;
  bool testColours = false;
  bool present = true;
  // When false, grass still draws but ignores the depth buffer. Used to
  // stamp isolated sprites for the pixel-exact check.
  bool grassDepthTest = true;
  // Seconds, drives the grass wind. Hold it fixed for repeatable captures.
  float time = 0.f;
};

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void init();
  void upload(const Scene& scene);
  void render(const Scene& scene, const CameraFrame& camera, const FrameParams& params);

  // Centre 640×360 of the low-res target, bottom-up RGBA8. This is the
  // snapped image, before the subpixel present shift.
  std::vector<std::uint8_t> readView();
  // Window framebuffer, bottom-up RGBA8. Call before swapping buffers.
  std::vector<std::uint8_t> readPresented(int width, int height);

 private:
  void destroyTargets();
  void createTargets();
  void createQuads();
  void drawPresent(const CameraFrame& camera, const FrameParams& params);
  unsigned makeColor(int w, int h);
  unsigned makeDepth(int w, int h, bool shadowCompare);

  unsigned fbo_ = 0;
  unsigned fboShadow_ = 0;
  unsigned color_ = 0;
  unsigned ground_ = 0;  // copy of color_ after terrain, read by grass
  unsigned depth_ = 0;
  unsigned shadow_ = 0;
  unsigned normal_ = 0;

  unsigned progTerrain_ = 0;
  unsigned progGrass_ = 0;
  unsigned progShadow_ = 0;
  unsigned progPresent_ = 0;

  unsigned grassVao_ = 0;
  unsigned grassCorner_ = 0;
  unsigned grassInst_ = 0;
  int grassCount_ = 0;

  unsigned presentVao_ = 0;
  unsigned presentVbo_ = 0;

  float worldMin_ = -45.f;
  float worldSize_ = 90.f;

  bool ready_ = false;
};

}  // namespace pxe
