#pragma once

#include "pxe/mesh.hpp"
#include "pxe/scene.hpp"
#include "pxe/scene_geometry.hpp"
#include "pxe/triangle_grid.hpp"

#include <glm/vec2.hpp>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <random>
#include <vector>

namespace pxe {

// Scattering sprites over a scene's surfaces. Every function draws from the
// generator it is given, in a fixed order, so a caller that seeds one
// generator and passes it to each in turn gets the same scene every time.

// Bridson Poisson disk in horizontal metres over the rectangle lo..hi,
// keeping only points `accept` likes. Every grid cell is tried as a fresh
// seed, so ground split by paving or water still fills. Separation is
// horizontal rather than along the slope so a steep bank does not pack extra
// sprites into one pixel.
std::vector<glm::vec2> poissonDisk(glm::vec2 lo, glm::vec2 hi, float minDist, std::mt19937& rng,
                                   const std::function<bool(float, float)>& accept);

// Slow 0..1 field so grass grows in taller and shorter patches; a uniform
// height makes every band edge the same fine comb.
float grassPatchiness(float x, float z);

struct GrassScatter {
  // Horizontal Poisson-disk radius for blade clumps, in metres.
  float clumpSpacing = 0.34f;
  // Spacing of the sparse detail tufts.
  float tuftSpacing = 2.f;
  bool tufts = true;
  // A 0..1 field over xz that sets each clump's height.
  std::function<float(float, float)> height = grassPatchiness;
};

// Adds grass sprites (surface kSurfGrass) over the grid's triangles: blade
// clumps carpet the ground at clump spacing so every band edge gets a
// continuous saw-tooth; tufts are a sparse second layer on top. Logs how
// many of each.
void scatterGrass(Scene& scene, const TriangleGrid& ground, const GrassScatter& params, std::mt19937& rng);

// Removes the grass sprites (surface kSurfGrass) rooted beneath the given
// surfaces' triangles in `geometry`, over the scene's bounds: ground that
// runs on under solid things would otherwise show its grass through their
// faces. Returns how many were removed.
std::size_t dropGrassUnder(Scene& scene, const SurfaceGeometry& geometry, std::initializer_list<Surface> surfaces);

// Adds leaf clusters (surface kSurfLeaf) over the crown triangles, about
// `density` per square metre, grown in clumps: clump centres are scattered
// over the crowns, and leaves gather around them with gaps between where the
// darker crown body shows. A clump shares a tilt, so its leaves tend to
// light together and a crown breaks into patches; each leaf adds its own
// tilt on top so patch edges stay leafy. Some leaves stand proud of the
// surface, which rags a crown's outline. Returns how many were added.
int scatterLeaves(Scene& scene, const std::vector<Triangle>& crowns, float density, std::mt19937& rng);

}  // namespace pxe
