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

// The Sleeping Fox (tavern_exterior.glb, 9 x 7 m) in the ruins: where its
// origin - the middle of its doorway's threshold, at floor level - stands,
// and its yaw (radians about +y; at 0 its door faces +z, toward the default
// view's camera). It stands alone on the open lawn far out to the front
// left of the ruins, about 47 m from where the hero starts (the default
// view's focus), out of the default view (yaw 180), turned 12.5 degrees
// so its door faces that view's camera. Settled by a search over the whole
// map (0.5 m and 2.5 degree steps, then 0.125 m round the best) with the
// real exterior and the ruins as built (docs/tasks/tavern/task.md):
// - Hard rules, all met: the door within 15 degrees of facing the default
//   camera; open lawn under all of its footprint, none of it dirt, from
//   -0.068 to 0.057 m (0.125 m of variation; its floor is just over the
//   highest, so its plinth, reaching 0.6 m down, meets the lawn all round);
//   the lawn before its door reached on foot from the hero's start over
//   ground the hero climbs (scene.ground cells, a 0.55 m step at most, its
//   feet and look-ahead, no deep water) in 51.1 m, round the building's
//   back and side, which face the ruins; and 0.3 m or more from everything,
//   seen from above.
// - Open lawn round it: nothing of ruins.glb within 4 m of its eaves,
//   chimney and sign (the nearest, TreeE's crown, is 6.83 m off); the
//   nearest ground that is not lawn (the map's edge) is 8.6 m off. The
//   campfire's dirt patch is 32.9 m away, and the fox in its bed
//   (kRuinsFoxSpot, a 1.53 m circle round it) 35.0 m.
// Of the spots with a 4 m ring of open lawn (most of those that pass), this is
// the most level; the lawn before the door is at -0.04, a 0.1 m step onto
// the threshold. All of it stays inside the ruins' bounds, so the sun's
// shadow map, fitted to them, covers it as it did before.
constexpr glm::vec3 kTavernSpot{-28.625f, 0.06f, 29.5f};
constexpr float kTavernYaw = 0.21816616f;
// Its footprint on the ground (xz, in its own coordinates; assets.md): the
// 9 x 7 m plinth's outline, with the threshold step at the front and the
// chimney's base at the +x end. Nothing else of it touches the ground.
constexpr glm::vec2 kTavernFootprint[] = {{-4.1f, 0.35f}, {-1.3f, 0.35f}, {-1.3f, 0.47f}, {1.3f, 0.47f},
                                          {1.3f, 0.35f},  {4.9f, 0.35f},  {4.9f, -3.6f},  {5.55f, -3.6f},
                                          {5.55f, -5.2f}, {4.9f, -5.2f},  {4.9f, -6.65f}, {-4.1f, -6.65f}};
// A point of the exterior, in its own coordinates, where it stands in the
// ruins; and a world point (xz) back in the exterior's coordinates.
glm::vec3 tavernPoint(glm::vec3 local);
glm::vec2 tavernLocal(glm::vec2 world);
// Whether a world point (xz) is inside the exterior's footprint.
bool inTavernFootprint(glm::vec2 world);

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
  // "ruins" (the Blender pond-and-pillars scene, with the Sleeping Fox on
  // its lawn), "hills", or "tavern" (the Sleeping Fox's room, inside).
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

pxe::Scene buildScene(const BuildOptions& options = {});

// What foxes need of a scene before it is built (their beds clear the grass
// as it is built): where beds may go, and what beds and walks keep clear of.
// laidOut() gives it as buildScene(options) lays the scene out; the hills
// and the tavern have none of it, as they have no foxes.
struct SceneLayout {
  // Each campfire's middle (xz); its dirt patch reaches kCampfireReach.
  std::vector<glm::vec2> campfires;
  // Everything standing in the scene, three corners to a triangle: all of
  // ruins.glb but its Ground (the lawn, and the dirt under the paving), and
  // every campfire's dirt patch, stones and logs, the shelter test's
  // shelter, the candles and the Sleeping Fox.
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
