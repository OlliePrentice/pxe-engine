#pragma once

#include "pxe/camera.hpp"
#include "pxe/config.hpp"
#include "pxe/fire.hpp"
#include "pxe/lighting.hpp"
#include "pxe/scene.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace pxe {

// One fire to draw this frame. The game owns its fires; the renderer keeps
// each one's flames going from frame to frame by `id`, so fires can be
// added, moved (a thrown torch) and removed as the game runs. `level` is
// how well it burns (0 out .. 1) and `smoke` how much it still smokes once
// out (1 just out .. 0).
struct FireDraw {
  std::uint32_t id = 0;
  glm::vec3 base{0.f};
  float level = 1.f;
  float smoke = 0.f;
};

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
  // Fire flames and their light, and the fires burning this frame.
  bool fire = true;
  std::vector<FireDraw> fires;
  // Sunbeams through the trees.
  bool rays = true;
  // Fireflies, and their light.
  bool fireflies = false;
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
  // Advance every fire this frame, choose which light the scene and which
  // of those cast shadows, and draw any shadow cube that needs it.
  void updateFires(const Scene& scene, const FrameParams& params);
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
  unsigned flashShadow_ = 0;  // lightning's shadow map, redrawn per strike
  unsigned heat_ = 0;         // fire heat (r), embers (g) and smoke (b), quantised by the edge pass
  unsigned fboHeat_ = 0;
  unsigned canopy_ = 0;  // tiling noise for the god rays' canopy gaps
  // Firelight. The kFireLights brightest fires light the scene; of those,
  // the kShadowCubes brightest also cast shadows, each from a cube of a
  // cube-map array - distance to the nearest surface on each face of a cube
  // round the fire. The scene does not move, so a cube is drawn when a fire
  // takes it and again only if the fire moves. A fire keeps its cube while
  // it stays among the brightest. When the cubes change hands, shadows fade
  // out on the fire giving one up and in on the fire taking it, over
  // kShadowFade seconds, so the light never jumps; a fire that is new this
  // frame takes its shadows at once.
  static constexpr int kFireLights = 16;
  static constexpr int kShadowCubes = 8;
  static constexpr float kShadowFade = 0.35f;
  struct ShadowCube {
    bool held = false;
    std::uint32_t fire = 0;  // id of the fire it belongs to
    float shadowed = 0.f;    // 0 no shadows .. 1 full shadows
    bool leaving = false;    // fading out before it is given up
    const Scene* drawnScene = nullptr;
    glm::vec3 drawnPos{0.f};
  };
  struct FireLight {
    glm::vec3 pos{0.f};
    float strength = 0.f;  // with the flicker
    int cube = -1;
    float shadowed = 0.f;
  };
  unsigned fireCubes_ = 0;  // GL_TEXTURE_CUBE_MAP_ARRAY, six layers per cube
  unsigned fireCubeDepth_ = 0;
  unsigned fboFireCube_ = 0;
  ShadowCube cubes_[kShadowCubes];
  std::vector<FireLight> fireLights_;  // this frame's
  float fireTime_ = -1.f;              // params.time of the last frame, < 0 none
  unsigned fboFlash_ = 0;

  // The toon programs this frame: one of toon_, by how many firelights.
  unsigned progMesh_ = 0;
  unsigned progGrass_ = 0;
  unsigned progShadow_ = 0;
  unsigned progPresent_ = 0;
  unsigned progEdge_ = 0;
  unsigned progWater_ = 0;
  struct ToonPrograms {
    unsigned mesh = 0, grass = 0, water = 0;
  };
  ToonPrograms toon_[2];  // [0] at most one firelight, [1] any number
  unsigned progFire_ = 0;
  unsigned progFireShadow_ = 0;
  unsigned fireVao_ = 0;
  unsigned fireVbo_ = 0;
  std::unordered_map<std::uint32_t, Fire> fires_;  // by FireDraw::id

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
  glm::vec3 flashLight_{0.f};
  const Scene* flashScene_ = nullptr;
  const Scene* shadowScene_ = nullptr;
  unsigned queries_[PassTimes::kCount] = {};
  PassTimes passTimes_;
  void beginPass(int pass);
  void endPass();
};

}  // namespace pxe
