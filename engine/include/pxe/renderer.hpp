#pragma once

#include "pxe/camera.hpp"
#include "pxe/scene.hpp"

#include <cstdint>
#include <vector>

namespace pxe {

// One shared wind field. Grass and leaves read the same values.
struct WindState {
  float dirX = 0.80f;
  float dirZ = 0.60f;
  float speed = 0.30f;
  float strength = 0.10f;
  float gust = 0.20f;
  float fps = 4.f;
  float rain = 0.f;
};

// Presets: 0 calm, 1 breezy, 2 storm, 3 rain. Used when a frame does not
// carry a blended override (screenshots, the snap self-test).
WindState windPreset(int weather);

struct FrameParams {
  int framebufferW = 1280;
  int framebufferH = 720;
  bool outlines = true;
  // 0 final, 1 nearest raw, 2 albedo, 3 normals, 4 unlit albedo (no upscale filter)
  int debugView = 0;
  // 0 calm, 1 breezy, 2 storm, 3 rain
  int weather = 0;
  float time = 0.f;
  // Time of day in hours, [0, 24). Drives the sun direction and the palette.
  float hour = 9.f;
  // When set, the blended wind below replaces windPreset(weather).
  bool windOverride = false;
  float windDirX = 0.80f;
  float windDirZ = 0.60f;
  float windSpeed = 0.30f;
  float windStrength = 0.10f;
  float windGust = 0.20f;
  float windFps = 4.f;
  float windRain = 0.f;
};

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void init();
  void uploadFoliage(const std::vector<FoliageInstance>& instances);
  void render(const Scene& scene, const CameraFrame& camera, const FrameParams& params);

  // Final low-res colour (bottom-up RGBA8), including outlines, water and rays.
  std::vector<std::uint8_t> readLowRes();
  // Window framebuffer, bottom-up RGBA8. Call before swapping buffers.
  std::vector<std::uint8_t> readPresented(int width, int height);
  int lowResWidth() const;
  int lowResHeight() const;

 private:
  void destroyTargets();
  void createTargets();
  void createPresentQuad();
  void updatePresentQuad(const CameraFrame& camera, const FrameParams& params);
  unsigned makeTarget(int w, int h, int internal, int format, int type, int filter);

  unsigned fboG_ = 0;
  unsigned fboLight_ = 0;
  unsigned fboComp_ = 0;
  unsigned fboShadow_ = 0;
  unsigned albedo_ = 0;
  unsigned normal_ = 0;
  unsigned linDepth_ = 0;
  unsigned depth_ = 0;
  unsigned colorA_ = 0;
  unsigned colorB_ = 0;
  unsigned shadowTex_ = 0;

  unsigned progG_ = 0;
  unsigned progFoliage_ = 0;
  unsigned progShadow_ = 0;
  unsigned progLight_ = 0;
  unsigned progOutline_ = 0;
  unsigned progWater_ = 0;
  unsigned progRays_ = 0;
  unsigned progPresent_ = 0;

  unsigned foliageVao_ = 0;
  unsigned foliageVbo_ = 0;
  unsigned foliageEbo_ = 0;
  unsigned foliageInst_ = 0;
  int foliageCount_ = 0;
  unsigned texGrass_ = 0;
  unsigned texFlower_ = 0;
  unsigned texLeaf_ = 0;

  unsigned presentVao_ = 0;
  unsigned presentVbo_ = 0;
  unsigned emptyVao_ = 0;

  bool ready_ = false;
};

}  // namespace pxe
