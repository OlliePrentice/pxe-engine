#pragma once

#include "pxe/scene.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace game {

// The ruins' campfire, on open lawn left of the pond (world xz).
constexpr glm::vec2 kRuinsCampfire{-5.f, -6.f};
// How far a campfire's scorched dirt patch reaches from its middle at most
// (its ragged rim is 0.82-0.96 m out; see addCampfire).
constexpr float kCampfireReach = 0.96f;
// Where the fox sleeps in the ruins (world xz): on open lawn 3.2 m from the
// campfire, on its pond side (+x) and behind it in the default view (yaw
// 180, looking toward -z), between the bank and the paving. Curled, it faces
// the fire. Standing, it has turned a quarter-turn left (in its clips), so it
// faces about 18 degrees right of straight at that view's camera: its face
// shows as it looks round. Every vertex of every frame of all five of its
// clips, at 2.4x and tilted to the slope, stays at least 0.2 m clear of the
// fire's stones, logs and dirt patch and of everything in ruins.glb
// (nearest: the paving to its right, 0.204 m as it lies down, then 0.309 m;
// the fire more than 1 m). The ground there is near planar: standing, its
// paws are 10-16 mm over it (9 mm on flat ground), so the curl's tilt suits
// the standing fox too. There is room ahead and to its left for its short
// walks (fox.hpp); to its right the fire and behind it the bank leave none.
constexpr glm::vec2 kRuinsFoxSpot{-2.f, -7.f};
// The second and third foxes' beds in play, found by search under the same
// rules: on open lawn, every vertex of every frame of all five clips, at 2.4x
// and tilted to the slope, at least 0.2 m clear of everything in ruins.glb,
// of the campfire's dirt patch, stones and logs, and of the fox in every
// other bed; and clear of the first fox's bed and of every route it can walk
// (and their own routes clear of those too), so it wakes and walks exactly
// as it does alone.
// The second sleeps 2.3 m from the campfire on the camera's side of it,
// facing it; standing, it faces left in the default view. Nearest: the
// paving behind it, 0.338 m as it stands up; the dirt patch 0.954 m; the
// first fox 2.46 m.
constexpr glm::vec2 kRuinsFoxSpot2{-5.5f, -3.75f};
// The third sleeps on the lawn in front of the paving, left of the view's
// middle, 7.3 m from the campfire, too far to face it: its curl faces -x, so
// standing it faces straight at the default view's camera. Nearest: the
// paving behind it, 0.370 m as it stands up; the bush BushD 0.478 m; the
// second fox 2.53 m.
constexpr glm::vec2 kRuinsFoxSpot3{-6.f, 1.25f};
constexpr float kRuinsFoxYaw3 = -1.57079633f;

// A fox's bed: where it lies (world xz) and the way its curl faces (yaw,
// radians; 0 faces +z).
struct FoxBed {
  glm::vec2 xz{0.f};
  float yaw = 0.f;
};

// The ground a bed's curl covers, for clearing the grass under it.
struct BedFootprint {
  glm::vec2 xz{0.f};
  float radius = 0.f;
};

struct BuildOptions {
  // "ruins" (the Blender pond-and-pillars scene) or "hills".
  std::string scene = "ruins";
  // Horizontal Poisson-disk radius for blade clumps, in metres. A clump is
  // about five pixels (0.3 m) wide, so this keeps band edges continuous.
  float minDist = 0.34f;
  // Spacing of the sparse detail tufts.
  float tuftDist = 2.f;
  bool tufts = true;
  // Leaf clusters per square metre of crown.
  float leafDensity = 6.f;
  // Eight isolated sprites instead of the field, for the pixel-exact check
  // (hills only).
  bool stamps = false;
  // Ruins only: add a campfire under a stone shelter and one under a tree
  // crown, to check that rain puts out only the fires open to it.
  bool shelterTest = false;
  // Ruins only: this many bare flames scattered over the lawn, to test many
  // fires at once.
  int extraFires = 0;
  // Ruins only: candles on flat stone tops and on the lawn, and, with the
  // shelter test, under its roof - to check that they light their
  // surroundings and that rain puts out only those open to it.
  bool candles = false;
  // Ruins only: the beds of the foxes shown - each one's xz and the radius
  // its curl covers - so no grass (clumps or tufts) grows up through them.
  // None (no foxes) leaves the grass as it always was.
  std::vector<BedFootprint> beds;
};

// A fire the scene starts with: where its flames rise from, whether a
// structure roofs it over (from `pxe::Scene::shelter`), so rain cannot reach
// it, and how big it is (1 a campfire, about 0.2 a candle; see
// pxe::FireDraw).
struct FireSite {
  glm::vec3 base{0.f};
  bool sheltered = false;
  float size = 1.f;
};

// A built scene: what the engine draws and queries, and what the game starts
// it with.
struct BuiltScene {
  pxe::Scene scene;
  // Fires the scene starts with. The game owns them from then on, and can
  // add, move and remove fires as it runs (see WorldClock).
  std::vector<FireSite> fires;
  // Where the camera starts.
  glm::vec3 focus{0.f};
  float yawDegrees = 45.f;
};

BuiltScene buildScene(const BuildOptions& options = {});

// What foxes need of a scene before it is built (their beds clear the grass
// as it is built): where beds may go, and what beds and walks keep clear of.
// laidOut() gives it as buildScene(options) lays the scene out; the hills
// have none of it, as they have no foxes.
struct SceneLayout {
  // Each campfire's middle (xz); its dirt patch reaches kCampfireReach.
  std::vector<glm::vec2> campfires;
  // Everything standing in the scene, three corners to a triangle: all of
  // ruins.glb but its Ground (the lawn, and the dirt under the paving), and
  // every campfire's dirt patch, stones and logs, the shelter test's
  // shelter and the candles.
  std::vector<glm::vec3> solids;
  // The top of the ground and all on it, as pxe::Scene::ground; and of the
  // lawn alone (the Ground's grass).
  pxe::HeightMap ground;
  pxe::HeightMap lawn;
  // Where beds are looked for (xz corners): round the ruins, about as far as
  // the default view shows.
  glm::vec2 bedsLo{0.f}, bedsHi{0.f};
};
SceneLayout laidOut(const BuildOptions& options);

}  // namespace game
