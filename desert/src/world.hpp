#pragma once

#include "pxe/scene.hpp"

#include <glm/vec2.hpp>

namespace desert {

// The ground: sand dunes over a 760 x 760 m square centred on the origin,
// so from anywhere in the play area (player.hpp's kPlayHalf, 80 m) its edge
// is at least 300 m away, past the far plane and fully hazed. The middle
// 240 x 240 m is a 1 m grid; the ring outside it has 4 m cells, joined to
// the fine grid's border vertices so there are no cracks between them.
constexpr float kGroundHalf = 380.f;
constexpr float kFineHalf = 120.f;
constexpr float kFineCell = 1.f;
constexpr float kCoarseCell = 4.f;

// The date palm (desert/assets/scenes/date_palm.glb) stands at the world
// origin, on the windward side of a low dune that rises behind it from the
// spawn's view. Its origin, the trunk base centre, goes at the ground height
// there. For collision it is a vertical cylinder round its trunk base: the
// circumradius of the base's seven sides (assets.md).
constexpr glm::vec2 kPalmSpot{0.f, 0.f};
constexpr float kPalmRadius = 0.40f;

// The dunes' height at world xz: a few layered, wind-aligned ridges, each
// with a long windward slope up to a sharp crest and a steep downwind face,
// from 2 to 6 m crest to trough.
float duneHeight(float x, float z);

// The drawn ground's height at world xz, within the fine grid (|x|, |z| up
// to kFineHalf, which holds the whole play area): duneHeight at the 1 m
// grid's corners, flat across each of its triangles, so whatever stands or
// walks on the ground sits exactly on the faces drawn.
float groundHeight(float x, float z);

// The desert's scene: the dunes, all sand, and the palm on them. The game
// owns it.
struct World {
  pxe::Scene scene;
};

// Builds and uploads the scene's geometry. Needs the window's GL context.
// Throws std::runtime_error if the palm's glb can't be read.
World buildWorld();

}  // namespace desert
