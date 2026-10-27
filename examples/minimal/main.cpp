// The smallest game on pxe: a procedural scene - rolling grass, a standing
// stone, a tree and a campfire on a stone slab - lit at a fixed hour and drawn
// in a window until it is closed or Escape is pressed. Q and E turn the view.
//
//   pxe_minimal                     open the window
//   pxe_minimal --capture out.png   draw a few frames offscreen, write the
//                                   low-res view to out.png, exit
//
// docs/engine-guide.md walks through this file.

#include "pxe/camera.hpp"
#include "pxe/config.hpp"
#include "pxe/image.hpp"
#include "pxe/input.hpp"
#include "pxe/lighting.hpp"
#include "pxe/mesh.hpp"
#include "pxe/renderer.hpp"
#include "pxe/scatter.hpp"
#include "pxe/scene.hpp"
#include "pxe/scene_geometry.hpp"
#include "pxe/triangle_grid.hpp"
#include "pxe/window.hpp"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kHour = 16.5f;              // the fixed time of day, 0-24
constexpr float kYaw = 45.f * kPi / 180.f;  // the view looks along +x +z
constexpr float kGroundHalf = 40.f;         // the ground runs -40..40 m in x and z
constexpr float kGroundCell = 0.5f;
constexpr int kCaptureFrames = 4;

// Rolling ground, about a metre from crest to trough.
float groundHeight(float x, float z) { return 0.5f * std::sin(0.21f * x + 0.4f) * std::cos(0.17f * z - 0.3f); }

glm::vec3 hex(std::uint32_t rgb) {
  return glm::vec3{static_cast<float>((rgb >> 16) & 0xff), static_cast<float>((rgb >> 8) & 0xff),
                   static_cast<float>(rgb & 0xff)} /
         255.f;
}

// Each surface's colours, shadow to lit, for each phase of the day. A surface
// left unset is black. The day colours are set by hand; golden hour, twilight
// and night are the day's under a tint, which keeps the example short. A
// game would choose every phase's colours by hand.
void setPalettes(pxe::Scene& scene) {
  struct Ramps {
    std::uint32_t band[3], detail[3], tip[3];
  };
  const auto plain = [](std::uint32_t shadow, std::uint32_t mid, std::uint32_t lit) {
    return Ramps{{shadow, mid, lit}, {shadow, mid, lit}, {shadow, mid, lit}};
  };
  Ramps day[pxe::kSurfCount] = {};
  day[pxe::kSurfGrass] = {{0x648474, 0x94b064, 0xacbc6c}, {0x74947c, 0x7c9c54, 0x8ca454}, {0x648474, 0xacbc6c, 0xc8d690}};
  day[pxe::kSurfStone] = plain(0x747480, 0xa49c8c, 0xbcac94);
  day[pxe::kSurfBark] = plain(0x6a6a7a, 0x8a8684, 0xbcac90);
  day[pxe::kSurfLeaf] = {{0x4c6e5c, 0x68905a, 0x88a850}, {0x58806a, 0x7ca25a, 0xaac65c}, {0x4c6e5c, 0x68905a, 0x88a850}};
  const glm::vec3 tint[pxe::kPhaseCount] = {
      {1.f, 1.f, 1.f}, {1.1f, 0.9f, 0.72f}, {0.5f, 0.52f, 0.72f}, {0.26f, 0.3f, 0.56f}};
  for (int ph = 0; ph < pxe::kPhaseCount; ++ph) {
    for (int s = 0; s < pxe::kSurfCount; ++s)
      for (int b = 0; b < 3; ++b) {
        scene.palette[ph][s].band[b] = hex(day[s].band[b]) * tint[ph];
        scene.palette[ph][s].detail[b] = hex(day[s].detail[b]) * tint[ph];
        scene.palette[ph][s].tip[b] = hex(day[s].tip[b]) * tint[ph];
      }
    scene.rim[ph] = hex(0xf5f0dc) * tint[ph];
    scene.ink[ph] = hex(0x4c4c66) * tint[ph];
  }
}

// A flat quad a-b-c-d with normal n, wound to face along n: meshes are drawn
// with back faces culled.
void addQuad(std::vector<pxe::Vertex>& v, std::vector<std::uint32_t>& idx, pxe::Surface surface, glm::vec3 a,
             glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
  if (glm::dot(glm::cross(b - a, c - a), n) < 0.f) std::swap(b, d);
  const auto base = static_cast<std::uint32_t>(v.size());
  for (const glm::vec3& p : {a, b, c, d}) v.push_back({p.x, p.y, p.z, n.x, n.y, n.z, static_cast<float>(surface)});
  idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// An upright box standing on the ground at (x, z), sunk a little so it
// never floats on a slope.
void addBox(pxe::SurfaceGeometry& geometry, pxe::Surface surface, float x, float z, glm::vec3 half) {
  const glm::vec3 centre{x, groundHeight(x, z) + half.y - 0.2f, z};
  const glm::vec3 axis[3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
  std::vector<pxe::Vertex> v;
  std::vector<std::uint32_t> idx;
  for (int a = 0; a < 3; ++a)
    for (float sign : {-1.f, 1.f}) {
      const glm::vec3 n = axis[a] * sign;
      const glm::vec3 u = axis[(a + 1) % 3] * half[(a + 1) % 3];
      const glm::vec3 w = axis[(a + 2) % 3] * half[(a + 2) % 3];
      const glm::vec3 c = centre + n * half[a];
      addQuad(v, idx, surface, c - u - w, c + u - w, c + u + w, c - u + w, n);
    }
  geometry.append(surface, v, idx);
}

// A round tree crown: a low-poly sphere of leaf surface with smooth normals.
// Returns its triangles, for scattering leaves over.
std::vector<pxe::Triangle> addCrown(pxe::SurfaceGeometry& geometry, glm::vec3 centre, float radius) {
  constexpr int kStacks = 8, kSlices = 12;
  std::vector<pxe::Vertex> v;
  std::vector<std::uint32_t> idx;
  for (int i = 0; i <= kStacks; ++i)
    for (int j = 0; j <= kSlices; ++j) {
      const float theta = kPi * static_cast<float>(i) / kStacks;
      const float phi = 2.f * kPi * static_cast<float>(j) / kSlices;
      const glm::vec3 n{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
      const glm::vec3 p = centre + radius * n;
      v.push_back({p.x, p.y, p.z, n.x, n.y, n.z, static_cast<float>(pxe::kSurfLeaf)});
    }
  for (int i = 0; i < kStacks; ++i)
    for (int j = 0; j < kSlices; ++j) {
      const auto a = static_cast<std::uint32_t>(i * (kSlices + 1) + j);
      const auto b = a + kSlices + 1;  // a row down
      idx.insert(idx.end(), {a, b + 1, b, a, a + 1, b + 1});
    }
  geometry.append(pxe::kSurfLeaf, v, idx);
  return pxe::trianglesOf(v, idx);
}

// The game's scene: what the engine draws, and where the game's own fire
// burns. The game owns both.
struct World {
  pxe::Scene scene;
  glm::vec3 fireBase{0.f};
  glm::vec3 focus{0.f};
};

// Builds and uploads the scene's geometry. Needs the window's GL context.
World buildWorld() {
  World world;
  pxe::Scene& scene = world.scene;
  setPalettes(scene);
  // The sun rises on the view's right and peaks ahead of it.
  scene.sunEast = {-std::cos(kYaw), 0.f, std::sin(kYaw)};
  scene.sunSouth = {std::sin(kYaw), 0.f, std::cos(kYaw)};

  // The ground: a height grid, all grass surface.
  std::vector<pxe::Vertex> groundVerts;
  std::vector<std::uint32_t> groundIdx;
  const int n = static_cast<int>(2.f * kGroundHalf / kGroundCell) + 1;
  for (int j = 0; j < n; ++j)
    for (int i = 0; i < n; ++i) {
      const float x = -kGroundHalf + static_cast<float>(i) * kGroundCell;
      const float z = -kGroundHalf + static_cast<float>(j) * kGroundCell;
      const float e = 0.05f;
      const glm::vec3 nrm = glm::normalize(glm::vec3{groundHeight(x - e, z) - groundHeight(x + e, z), 2.f * e,
                                                     groundHeight(x, z - e) - groundHeight(x, z + e)});
      groundVerts.push_back({x, groundHeight(x, z), z, nrm.x, nrm.y, nrm.z, static_cast<float>(pxe::kSurfGrass)});
    }
  for (int j = 0; j + 1 < n; ++j)
    for (int i = 0; i + 1 < n; ++i) {
      const auto i00 = static_cast<std::uint32_t>(j * n + i);
      const auto i10 = i00 + 1;
      const auto i01 = i00 + static_cast<std::uint32_t>(n);
      const auto i11 = i01 + 1;
      groundIdx.insert(groundIdx.end(), {i00, i11, i10, i00, i01, i11});
    }

  pxe::SurfaceGeometry geometry;
  geometry.append(pxe::kSurfGrass, groundVerts, groundIdx);
  // A standing stone, a tree, and a stone slab for the campfire.
  addBox(geometry, pxe::kSurfStone, 3.f, -1.f, {0.6f, 1.6f, 0.5f});
  addBox(geometry, pxe::kSurfBark, -3.f, 2.f, {0.25f, 2.2f, 0.25f});
  const std::vector<pxe::Triangle> crown =
      addCrown(geometry, {-3.f, groundHeight(-3.f, 2.f) + 4.2f, 2.f}, 1.8f);
  addBox(geometry, pxe::kSurfStone, 1.f, 3.f, {0.8f, 0.28f, 0.8f});
  world.fireBase = {1.f, groundHeight(1.f, 3.f) + 0.36f, 3.f};
  world.focus = {0.f, groundHeight(0.f, 0.f) + 1.f, 1.f};

  pxe::buildSceneMaps(scene, geometry);
  pxe::uploadScene(scene, geometry);

  // Grass and leaves from one seeded generator, in a fixed order, so the
  // scene comes out the same on every run. The scatter defaults are tuned
  // at 16 pixels per metre; scale them to the engine's pixel density.
  std::mt19937 rng(7);
  const float density = pxe::RenderConfig::kPixelsPerUnit / 16.f;
  pxe::GrassScatter grass;
  grass.clumpSpacing /= density;
  grass.tuftSpacing /= density;
  const pxe::TriangleGrid ground(pxe::trianglesOf(groundVerts, groundIdx), 1.f);
  pxe::scatterGrass(scene, ground, grass, rng);
  pxe::dropGrassUnder(scene, geometry, {pxe::kSurfStone, pxe::kSurfBark});
  pxe::scatterLeaves(scene, crown, 6.f * density * density, rng);
  return world;
}

// One frame's draw list: everything the renderer needs, built fresh.
pxe::FrameParams frameParams(const World& world, glm::ivec2 framebuffer, float time) {
  pxe::FrameParams params;
  params.framebufferW = framebuffer.x;
  params.framebufferH = framebuffer.y;
  params.time = time;
  params.lighting = pxe::lightingAt(world.scene, kHour, pxe::Weather{});
  params.fires.push_back({1, world.fireBase, 1.f, 0.f, 1.f});
  return params;
}

}  // namespace

int main(int argc, char** argv) {
  std::string capture;
  if (argc == 3 && std::string(argv[1]) == "--capture") {
    capture = argv[2];
  } else if (argc != 1) {
    std::fprintf(stderr, "usage: pxe_minimal [--capture out.png]\n");
    return 1;
  }

  try {
    // The window first: it makes the GL context everything after it needs.
    pxe::WindowOptions options;
    options.width = 1920;
    options.height = 1080;
    options.title = "pxe minimal";
    options.visible = capture.empty();
    options.vsync = capture.empty();
    pxe::Window window(options);

    pxe::Renderer renderer;
    renderer.init();

    World world = buildWorld();
    renderer.upload(world.scene);

    pxe::Camera camera;
    camera.setYawRadians(kYaw);
    camera.setTarget(world.focus);

    if (!capture.empty()) {
      // A few frames at fixed times, so a capture is the same on every run.
      for (int f = 0; f < kCaptureFrames; ++f) {
        const pxe::FrameParams params = frameParams(world, window.framebufferSize(), static_cast<float>(f) / 60.f);
        renderer.render(world.scene, camera.frame(renderer.rtW(), renderer.rtH()), params);
      }
      const bool ok = pxe::writePng(capture, renderer.viewW(), renderer.viewH(), renderer.readView());
      std::fprintf(stderr, "pxe_minimal: %s %s\n", ok ? "wrote" : "failed to write", capture.c_str());
      return ok ? 0 : 1;
    }

    pxe::Input input;
    double previous = window.time();
    while (!window.shouldClose()) {
      const double now = window.time();
      const float dt = static_cast<float>(now - previous);
      previous = now;

      window.pollEvents();
      input.update(window);
      if (input.keyPressed(pxe::Key::kEscape)) window.requestClose();
      if (input.keyPressed(pxe::Key::kQ)) camera.rotateSteps(-1);
      if (input.keyPressed(pxe::Key::kE)) camera.rotateSteps(1);
      camera.update(dt);

      const pxe::FrameParams params = frameParams(world, window.framebufferSize(), static_cast<float>(now));
      renderer.render(world.scene, camera.frame(renderer.rtW(), renderer.rtH()), params);
      window.swap();
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "pxe_minimal: %s\n", e.what());
    return 1;
  }
  return 0;
}
