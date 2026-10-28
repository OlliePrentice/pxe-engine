#pragma once

#include "pxe/camera.hpp"
#include "pxe/config.hpp"
#include "pxe/fire.hpp"
#include "pxe/firegrid.hpp"
#include "pxe/lighting.hpp"
#include "pxe/scene.hpp"
#include "pxe/skinned.hpp"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace pxe {

// One fire to draw this frame. The game owns its fires; the renderer keeps
// each one's flames going from frame to frame by `id`, so fires can be
// added, moved (a thrown torch) and removed as the game runs. `level` is
// how well it burns (0 out .. 1) and `smoke` how much it still smokes once
// out (1 just out .. 0). `size` scales the flames: 1 a campfire, about 0.2
// a candle. A campfire lights the scene with the forward firelights (with
// shadows, for the brightest); a smaller fire - a candle, a torch - lights
// only through the point lights, without shadows.
struct FireDraw {
  std::uint32_t id = 0;
  glm::vec3 base{0.f};
  float level = 1.f;
  float smoke = 0.f;
  float size = 1.f;
};

// A small light without shadows, lit in the post pass from the depth and
// normals already drawn, like t3ssel8r's light volumes: any number of them,
// each costing only the screen tiles it reaches. A lit surface moves toward
// its colour in full light of the light's kind (Lighting::fireflyBand or
// fireBand), by `strength`, fading smoothly to nothing at `radius` metres.
struct PointLight {
  enum Kind : int { kFirefly = 0, kFire = 1 };
  glm::vec3 pos{0.f};
  float radius = 1.f;
  float strength = 1.f;
  Kind kind = kFire;
};

// A character posed this frame: its model's mesh, drawn through every pass
// with its own joint matrices (SkinnedModel::skin), which must last until
// the frame is drawn. Any number of characters may share one mesh.
struct ActorDraw {
  const SkinnedMesh* mesh = nullptr;
  std::span<const glm::mat4> joints;
};

struct FrameParams {
  // The window's framebuffer in pixels (Window::framebufferSize()), which
  // the view is presented into. Required when `present` is true: render
  // throws std::invalid_argument if either is not positive. Unused, and may
  // stay 0, when `present` is false.
  int framebufferW = 0;
  int framebufferH = 0;
  bool showGrass = true;
  bool magentaGrass = false;
  bool showNdl = false;
  bool testColours = false;
  // Scale the view into the window's framebuffer at the end of the frame.
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
  // Point lights the game adds this frame, besides those the renderer makes
  // for fireflies and small fires.
  std::vector<PointLight> lights;
  // Grass fire (see FireGrid): once on, the fire grid over the lawn runs
  // every frame - spreading, burning the grass, put out by rain - and the
  // grass, the lawn and the fire's flames and light draw from it. Off, none
  // of it is touched. `igniteGrass` sets the grass alight round each world
  // xz point this frame, and `gridStrokes` paints brush strokes into it.
  bool grassFire = false;
  std::vector<glm::vec2> igniteGrass;
  std::vector<FireGrid::Stroke> gridStrokes;
  // Seconds a grass fire set alight keeps spreading before it dies out
  // (FireGrid::setLifetime); 0 until there is nothing left to burn.
  float grassFireLife = 0.f;
  // Sun or moon, palettes and clouds for this frame (see lightingAt).
  Lighting lighting;
  // Characters posed this frame, skinned in the vertex shaders, their
  // vertices' surfaces kActorSurface + an index into actorMaterials. They
  // draw, cast shadows and reflect like the scene's meshes; the scene's own
  // shadows stay cached, with the characters' laid over them.
  std::vector<ActorDraw> actors;
  ActorMaterial actorMaterials[kActorMaterials];
  // Drawn with the characters but casting no shadow, already in world space:
  // a sword's glowing smear.
  std::vector<const Mesh*> actorEffects;
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
  // Take a scene to draw: its grass goes to the GPU, and every cache drawn
  // from the scene (sun, lightning and firelight shadow maps) and the fire
  // state are dropped. Call again whenever the game builds or swaps a scene.
  // With `keepGrassFire`, the grass fire's grid (its burnt grass) is kept
  // rather than dropped: for coming back to the scene it was built for from
  // one drawn without a grass fire (FrameParams::grassFire never set), so
  // the grass burnt before stays burnt. The grid must only ever advance in
  // the scene it was built for.
  void upload(const Scene& scene, bool keepGrassFire = false);
  // Just the grass, after the game has changed scene.grass (say, cleared
  // some blades); everything else upload() set stays.
  void uploadGrass(const Scene& scene);
  // Put out the grass fire - every flame, ember and burning crown - leaving
  // the grass it burnt burnt (FireGrid::quench).
  void quenchGrassFire() { grid_.quench(); }
  // A point of the window (framebuffer pixels from its top left, `fbW` x
  // `fbH`) as presented for `camera`: the view ray through it starts at the
  // returned point on the camera's plane and runs along camera.forward.
  glm::vec3 rayThrough(const CameraFrame& camera, int fbW, int fbH, glm::vec2 px) const;
  // And back: where a world point shows in the window.
  glm::vec2 windowPoint(const CameraFrame& camera, int fbW, int fbH, glm::vec3 world) const;
  // Draw a frame. `scene` must be the scene last passed to upload(): the
  // renderer keeps shadow maps drawn from it between frames and redraws them
  // only when the light changes or upload() is called again, never by
  // checking which scene it is given. Throws std::invalid_argument, before
  // drawing anything, if params.present is set without a positive
  // framebuffer size.
  void render(const Scene& scene, const CameraFrame& camera, const FrameParams& params);

  // Centre RenderConfig::kViewW x kViewH of the low-res target (the view
  // without its padding), bottom-up RGBA8. This is the snapped image,
  // before the subpixel present shift.
  std::vector<std::uint8_t> readView();
  // Window framebuffer, bottom-up RGBA8. Call before swapping buffers.
  std::vector<std::uint8_t> readPresented(int width, int height);

 private:
  void destroyTargets();
  void createTargets();
  void createQuads();
  // Advance every fire this frame, choose which light the scene and which
  // of those cast shadows, and draw any shadow cube that needs it. Fires
  // that light only through the point lights go into pointLights_.
  void updateFires(const Scene& scene, const FrameParams& params);
  // Cut the point lights into per-tile lists for the edge pass and upload
  // them; returns how many lights there are.
  int uploadPointLights(const CameraFrame& camera);
  void drawPresent(const CameraFrame& camera, const FrameParams& params);
  // Draw the scene's meshes (and sprites, if shown) with the given mirror.
  void drawMeshes(const Scene& scene, const FrameParams& params, const glm::mat4& viewProj, const glm::mat4& mirror,
                  float clipBelow);
  void drawSprites(const Scene& scene, const CameraFrame& camera, const FrameParams& params,
                   const glm::mat4& viewProj, const glm::mat4& mirror, float spriteFlip, float clipBelow);
  unsigned makeColor(int w, int h);
  unsigned makeDepth(int w, int h, bool shadowCompare);

  unsigned fbo_ = 0;
  unsigned fboShadow_ = 0;
  // The scene's own sun shadows, kept while nothing but characters moves;
  // copied into shadow_ each frame a character is about, and drawn over.
  unsigned shadowStatic_ = 0;
  unsigned fboShadowStatic_ = 0;
  bool shadowHadActors_ = false;
  bool shadowStaticValid_ = false;  // shadowStatic_ holds the scene's map as it stands
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
  // Firelight. The kFireLights brightest campfires light the scene in the
  // toon shaders. A fire holds one of the kFireLights slots while it is
  // among the brightest; when the slots change hands, a fire's light fades
  // out of its slot into the point lights, or in the other way, over
  // kShadowFade seconds, so no light jumps. Fires without a slot light
  // through the point lights, without shadows. Of the slotted fires, the
  // kShadowCubes brightest also cast shadows, each from a cube of a
  // cube-map array - distance to the nearest surface on each face of a cube
  // round the fire. The scene does not move, so a cube is drawn when a fire
  // takes it and again only if the fire moves. A fire keeps its cube while
  // it stays among the brightest. When the cubes change hands, shadows fade
  // out on the fire giving one up and in on the fire taking it, over
  // kShadowFade seconds, so the light never jumps; a fire that is new this
  // frame takes its slot and its shadows at once.
  static constexpr int kFireLights = 16;
  static constexpr int kShadowCubes = 8;
  static constexpr float kShadowFade = 0.35f;
  struct ShadowCube {
    bool held = false;
    std::uint32_t fire = 0;  // id of the fire it belongs to
    float shadowed = 0.f;    // 0 no shadows .. 1 full shadows
    bool leaving = false;    // fading out before it is given up
    bool drawn = false;      // the cube holds the uploaded scene seen from drawnPos
    glm::vec3 drawnPos{0.f};
  };
  struct LightSlot {
    bool held = false;
    std::uint32_t fire = 0;
    float weight = 0.f;   // share of its light in the slot, the rest in the point lights
    bool leaving = false;  // fading out before it is given up
  };
  LightSlot slots_[kFireLights];
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
  // This frame's point lights, cut into screen tiles of kTile pixels: a
  // buffer texture of lights (two texels each) and one of tile lists (first
  // index and count per tile, then the light indices).
  static constexpr int kTile = 16;
  std::vector<PointLight> pointLights_;
  unsigned pointBuf_ = 0, pointTex_ = 0;
  unsigned tileBuf_ = 0, tileTex_ = 0;
  int tilesX_ = 0;
  float fireTime_ = -1.f;              // params.time of the last frame, < 0 none
  // The wind's phase: advances faster the stronger it blows, accumulated
  // so a change of strength never makes the grass jump.
  float windPhase_ = 0.f;
  float windTime_ = -1.f;
  unsigned fboFlash_ = 0;

  // The toon programs this frame: one of toon_, by how many firelights.
  unsigned progMesh_ = 0;
  unsigned progGrass_ = 0;
  unsigned progShadow_ = 0;
  unsigned progShadowGrid_ = 0;  // crowns burning away: reads the grass fire's burn texture
  unsigned progPresent_ = 0;
  unsigned progEdge_ = 0;      // this frame's: one of edge_
  // [grass fire]: [1] adds the grid's glow and darkens charred ground in
  // the firelight; [0] is the edge pass as it always was.
  unsigned edge_[2] = {};
  unsigned progWater_ = 0;
  struct ToonPrograms {
    unsigned mesh = 0, grass = 0, water = 0;
  };
  // [grass fire][firelights]: [0][..] without the fire grid, [1][..] reading
  // its burn texture; [..][0] at most one firelight, [..][1] any number.
  ToonPrograms toon_[2][2];
  FireGrid grid_;
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

  // The sky seen through a room's windows: a pane just outside each one
  // (Scene::windows), built on upload, drawn after the meshes and nowhere
  // else - not into any shadow map.
  unsigned progSky_ = 0;
  unsigned skyVao_ = 0;
  unsigned skyVbo_ = 0;
  int skyVertices_ = 0;


  bool ready_ = false;
  bool profiling_ = false;
  // The shadow maps are only redrawn when the light changes or they are
  // invalidated: by upload() or when their textures are recreated.
  glm::vec3 shadowLight_{0.f};
  glm::vec3 flashLight_{0.f};
  bool shadowValid_ = false;  // the sun's map holds the uploaded scene from shadowLight_
  bool flashValid_ = false;   // flashShadow_ holds the uploaded scene from flashLight_
  unsigned queries_[PassTimes::kCount] = {};
  PassTimes passTimes_;
  void beginPass(int pass);
  void endPass();
};

}  // namespace pxe
