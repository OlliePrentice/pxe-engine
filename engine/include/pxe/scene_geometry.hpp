#pragma once

#include "pxe/gltf.hpp"
#include "pxe/mesh.hpp"
#include "pxe/scene.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace pxe {

// A scene's geometry before it is uploaded: one vertex and index list per
// surface, so each surface draws as one mesh. Built in stages: append the
// loaded primitives, add any further geometry to the lists directly, then
// hand it to buildSceneMaps and uploadScene.
struct SurfaceGeometry {
  std::vector<Vertex> vertices[kSurfCount];
  std::vector<std::uint32_t> indices[kSurfCount];
  // The bounds of everything appended with append(), and only that:
  // geometry added to the lists directly does not grow them.
  glm::vec3 boundsMin{1e9f};
  glm::vec3 boundsMax{-1e9f};

  // Adds the primitive to its surface's lists, its indices offset past the
  // vertices already there.
  void append(const GltfPrimitive& primitive);
  void append(Surface surface, const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
  // The corners of one surface's triangles, three to a triangle, in index
  // order.
  std::vector<glm::vec3> corners(Surface surface) const;
};

// Gives the scene the maps it is queried through: `shelter` from the stone
// (built structure keeps the rain off; tree crowns let it through),
// `ground` from the grass, dirt, stone, bark and foam, and `waterTop` from
// the water.
void buildSceneMaps(Scene& scene, const SurfaceGeometry& geometry);

// Uploads the geometry: a mesh per non-empty surface, the water's going to
// `scene.water`, and the water's level (half-way up its sheets) and extent.
// Grows the scene's bounds to hold the appended primitives.
void uploadScene(Scene& scene, const SurfaceGeometry& geometry);

}  // namespace pxe
