#pragma once

#include "pxe/camera.hpp"
#include "pxe/config.hpp"
#include "pxe/lighting.hpp"
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
  // Light rims and dark outlines on stone and bark.
  bool edges = true;
  // Sun or moon, palettes and clouds for this frame (see lightingAt).
  Lighting lighting;
};

// GPU milliseconds per pass for the last frame, when profiling is on.
struct PassTimes {
  enum Pass { kShadow, kReflection, kMeshes, kWater, kSprites, kEdge, kPresent, kCount };
  double ms[kCount] = {};
  static const char* name(int pass);
};

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void init();
  // Time each pass on the GPU (blocks at the end of every frame; for --bench).
  void setProfiling(bool on) { profiling_ = on; }
  const PassTimes& passTimes() const { return passTimes_; }
  int viewW() const { return RenderConfig::kViewW; }
  int viewH() const { return RenderConfig::kViewH; }
  int rtW() const { return RenderConfig::kViewW + 2 * RenderConfig::kPad; }
  int rtH() const { return RenderConfig::kViewH + 2 * RenderConfig::kPad; }
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
  // Draw the scene's meshes (and sprites, if shown) with the given mirror.
  void drawMeshes(const Scene& scene, const glm::mat4& viewProj, const glm::mat4& mirror, float clipBelow);
  void drawSprites(const Scene& scene, const CameraFrame& camera, const FrameParams& params,
                   const glm::mat4& viewProj, const glm::mat4& mirror, float spriteFlip, float clipBelow);
  unsigned makeColor(int w, int h);
  unsigned makeDepth(int w, int h, bool shadowCompare);

  unsigned fbo_ = 0;
  unsigned fboShadow_ = 0;
  unsigned color_ = 0;
  unsigned normals_ = 0;  // normal + surface, for the edge pass
  unsigned final_ = 0;    // colour after the edge pass
  unsigned fboFinal_ = 0;
  // Water: the mirrored scene, and the scene + depth under the water.
  unsigned refl_ = 0;
  unsigned reflDepth_ = 0;
  unsigned fboRefl_ = 0;
  unsigned bed_ = 0;
  unsigned bedDepth_ = 0;
  unsigned ground_ = 0;  // copy of color_ after terrain, read by grass
  unsigned depth_ = 0;
  unsigned shadow_ = 0;

  unsigned progMesh_ = 0;
  unsigned progGrass_ = 0;
  unsigned progShadow_ = 0;
  unsigned progPresent_ = 0;
  unsigned progEdge_ = 0;
  unsigned progWater_ = 0;

  unsigned grassVao_ = 0;
  unsigned grassCorner_ = 0;
  unsigned grassInst_ = 0;
  int grassCount_ = 0;

  unsigned presentVao_ = 0;
  unsigned presentVbo_ = 0;


  bool ready_ = false;
  bool profiling_ = false;
  // The shadow map is only redrawn when the light or the scene changes.
  glm::vec3 shadowLight_{0.f};
  const Scene* shadowScene_ = nullptr;
  unsigned queries_[PassTimes::kCount] = {};
  PassTimes passTimes_;
  void beginPass(int pass);
  void endPass();
};

}  // namespace pxe
