// The smallest game on pxe: a procedural scene - rolling grass, a standing
// stone, a tree and a campfire on a stone slab - lit at a fixed hour and drawn
// in a window until it is closed or Escape is pressed. Q and E turn the view.
// With --first-person it is seen from standing height through a perspective
// camera instead, looking at the stone and the tree: the mouse looks around,
// WASD walks (Shift sprints), Escape lets the cursor go and a click takes it
// back, and Escape with the cursor free quits.
//
//   pxe_minimal                     open the window
//   pxe_minimal --capture out.png   draw a few frames offscreen, write the
//                                   low-res view to out.png, exit
//   pxe_minimal --first-person      the view from standing height
//   pxe_minimal --hour H            light it at hour H (0-24), not 16.5
//
// docs/engine-guide.md walks through this file.

#include "pxe/camera.hpp"
#include "pxe/first_person_camera.hpp"
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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kHour = 16.5f;              // the time of day, 0-24, unless --hour says
constexpr float kYaw = 45.f * kPi / 180.f;  // the view looks along +x +z
constexpr float kGroundHalf = 40.f;         // the ground runs -40..40 m in x and z
constexpr float kGroundCell = 0.5f;
constexpr int kCaptureFrames = 4;
// The first-person view: an eye at standing height, reaching just past the
// ground's far edges so they fade into the sky, turned by the mouse and
// walked by the keys.
constexpr float kEyeHeight = 1.7f;
constexpr float kFirstPersonFar = 50.f;
constexpr float kLookPerPoint = 0.0015f;        // radians per window point of mouse motion
constexpr float kMaxPitch = 85.f * kPi / 180.f;  // up or down
constexpr float kWalkSpeed = 3.5f;              // metres a second
constexpr float kSprintSpeed = 6.f;             // with Shift held
constexpr float kWalkHalf = kGroundHalf - 2.f;  // the walk stays 2 m inside the ground's edges
constexpr float kEyeEase = 10.f;                // how fast the eye settles to standing height, per second

// Rolling ground, about a metre from crest to trough.
float groundHeight(float x, float z) { return 0.5f * std::sin(0.21f * x + 0.4f) * std::cos(0.17f * z - 0.3f); }

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

// The game's scene: what the engine draws, where the game's own fire
// burns, and where its cameras start. The game owns all of it.
struct World {
  pxe::Scene scene;
  glm::vec3 fireBase{0.f};
  glm::vec3 focus{0.f};
  // The first-person eye, and the point it looks at to start with.
  glm::vec3 eye{0.f};
  glm::vec3 lookAt{0.f};
};

// Builds and uploads the scene's geometry. Needs the window's GL context.
World buildWorld() {
  World world;
  // A new scene starts with the engine's default palettes for every surface
  // and phase of the day, so the example sets none.
  pxe::Scene& scene = world.scene;
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
  // Facing the stone, the campfire and the tree, the stone on the left.
  world.eye = {-4.f, groundHeight(-4.f, -7.f) + kEyeHeight, -7.f};
  world.lookAt = {0.f, groundHeight(0.f, 0.5f) + 1.4f, 0.5f};

  pxe::buildSceneMaps(scene, geometry);
  pxe::uploadScene(scene, geometry);

  // Grass and leaves from one seeded generator, in a fixed order, so the
  // scene comes out the same on every run. The scatter defaults are already
  // at the engine's pixel density.
  std::mt19937 rng(7);
  const pxe::TriangleGrid ground(pxe::trianglesOf(groundVerts, groundIdx), 1.f);
  pxe::scatterGrass(scene, ground, pxe::GrassScatter{}, rng);
  pxe::dropGrassUnder(scene, geometry, {pxe::kSurfStone, pxe::kSurfBark});
  pxe::scatterLeaves(scene, crown, pxe::kDefaultLeafDensity, rng);
  return world;
}

// The example's own walker: WASD along the view's flat forward and right,
// Shift to sprint, held inside the ground's edges, the eye eased to standing
// height over the rolling ground so its slopes don't jolt the view. Nothing
// stops it walking through the stone, the slab or the tree.
void walk(pxe::FirstPersonCamera& eyeCamera, const pxe::Input& input, float dt) {
  const float yaw = eyeCamera.yawRadians();
  const glm::vec2 forward{std::sin(yaw), std::cos(yaw)};  // in x and z
  const glm::vec2 right{-std::cos(yaw), std::sin(yaw)};
  glm::vec2 move{0.f};
  if (input.keyDown(pxe::Key::kW)) move += forward;
  if (input.keyDown(pxe::Key::kS)) move -= forward;
  if (input.keyDown(pxe::Key::kD)) move += right;
  if (input.keyDown(pxe::Key::kA)) move -= right;
  glm::vec3 eye = eyeCamera.eye();
  if (glm::dot(move, move) > 0.f) {
    const bool sprint = input.keyDown(pxe::Key::kLeftShift) || input.keyDown(pxe::Key::kRightShift);
    const glm::vec2 step = glm::normalize(move) * (sprint ? kSprintSpeed : kWalkSpeed) * dt;
    eye.x = std::clamp(eye.x + step.x, -kWalkHalf, kWalkHalf);
    eye.z = std::clamp(eye.z + step.y, -kWalkHalf, kWalkHalf);
  }
  const float standing = groundHeight(eye.x, eye.z) + kEyeHeight;
  eye.y += (standing - eye.y) * (1.f - std::exp(-kEyeEase * dt));
  eyeCamera.setEye(eye);
}

// One frame's draw list: everything the renderer needs, built fresh.
pxe::FrameParams frameParams(const World& world, glm::ivec2 framebuffer, float time, float hour) {
  pxe::FrameParams params;
  params.framebufferW = framebuffer.x;
  params.framebufferH = framebuffer.y;
  params.time = time;
  params.lighting = pxe::lightingAt(world.scene, hour, pxe::Weather{});
  params.fires.push_back({1, world.fireBase, 1.f, 0.f, 1.f});
  return params;
}

}  // namespace

int main(int argc, char** argv) {
  std::string capture;
  bool firstPerson = false;
  float hour = kHour;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    bool ok = true;
    if (arg == "--capture" && i + 1 < argc) {
      capture = argv[++i];
    } else if (arg == "--first-person") {
      firstPerson = true;
    } else if (arg == "--hour" && i + 1 < argc) {
      char* end = nullptr;
      hour = std::strtof(argv[++i], &end);
      ok = *end == '\0' && hour >= 0.f && hour <= 24.f;
    } else {
      ok = false;
    }
    if (!ok) {
      std::fprintf(stderr, "usage: pxe_minimal [--capture out.png] [--first-person] [--hour 0-24]\n");
      return 1;
    }
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
    // The renderer is made for one camera for its whole life: the first-person
    // view needs perspective mode, the orbiting one orthographic.
    renderer.init(firstPerson ? pxe::RenderMode::kPerspective : pxe::RenderMode::kOrthographic);

    World world = buildWorld();
    renderer.upload(world.scene);

    pxe::Camera camera;
    camera.setYawRadians(kYaw);
    camera.setTarget(world.focus);

    // The first-person camera, turned toward world.lookAt. A positive pitch
    // looks down.
    pxe::FirstPersonCamera eyeCamera;
    eyeCamera.setEye(world.eye);
    eyeCamera.setFarPlane(kFirstPersonFar);
    const glm::vec3 look = world.lookAt - world.eye;
    eyeCamera.setYawRadians(std::atan2(look.x, look.z));
    eyeCamera.setPitchRadians(-std::atan2(look.y, std::hypot(look.x, look.z)));
    // Draw a frame through whichever camera this run looks through: each
    // has its own frame type, for the renderer's mode.
    auto renderFrame = [&](const pxe::FrameParams& params) {
      if (firstPerson) {
        renderer.render(world.scene, eyeCamera.frame(renderer.rtW(), renderer.rtH()), params);
      } else {
        renderer.render(world.scene, camera.frame(renderer.rtW(), renderer.rtH()), params);
      }
    };

    if (!capture.empty()) {
      // A few frames at fixed times, so a capture is the same on every run.
      for (int f = 0; f < kCaptureFrames; ++f) {
        const pxe::FrameParams params =
            frameParams(world, window.framebufferSize(), static_cast<float>(f) / 60.f, hour);
        renderFrame(params);
      }
      const bool ok = pxe::writePng(capture, renderer.viewW(), renderer.viewH(), renderer.readView());
      std::fprintf(stderr, "pxe_minimal: %s %s\n", ok ? "wrote" : "failed to write", capture.c_str());
      return ok ? 0 : 1;
    }

    pxe::Input input;
    // Mouse look holds the cursor in the window.
    if (firstPerson) window.setCursorCaptured(true);
    double previous = window.time();
    while (!window.shouldClose()) {
      const double now = window.time();
      const float dt = static_cast<float>(now - previous);
      previous = now;

      window.pollEvents();
      input.update(window);
      if (firstPerson) {
        // Escape lets the cursor go, and quits once it is free; a click
        // takes it back. The view looks and walks only while it is held.
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
          // Moving the mouse right turns right (a smaller yaw), down looks down.
          const glm::vec2 turn = input.mouseDelta() * kLookPerPoint;
          eyeCamera.setYawRadians(eyeCamera.yawRadians() - turn.x);
          eyeCamera.setPitchRadians(std::clamp(eyeCamera.pitchRadians() + turn.y, -kMaxPitch, kMaxPitch));
          walk(eyeCamera, input, dt);
        }
      } else {
        if (input.keyPressed(pxe::Key::kEscape)) window.requestClose();
        if (input.keyPressed(pxe::Key::kQ)) camera.rotateSteps(-1);
        if (input.keyPressed(pxe::Key::kE)) camera.rotateSteps(1);
        camera.update(dt);
      }

      // A minimised window can have a 0x0 framebuffer, and render throws when
      // asked to present at that size: skip drawing, and wait rather than spin.
      const glm::ivec2 framebuffer = window.framebufferSize();
      if (framebuffer.x <= 0 || framebuffer.y <= 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        continue;
      }
      const pxe::FrameParams params = frameParams(world, framebuffer, static_cast<float>(now), hour);
      renderFrame(params);
      window.swap();
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "pxe_minimal: %s\n", e.what());
    return 1;
  }
  return 0;
}
