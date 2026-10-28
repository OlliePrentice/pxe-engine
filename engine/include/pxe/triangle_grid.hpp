#pragma once

#include "pxe/mesh.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace pxe {

// A triangle's corners and their (smooth) vertex normals.
struct Triangle {
  glm::vec3 p[3];
  glm::vec3 n[3];
};

// The triangles of an indexed vertex list, in index order (three indices to
// a triangle; a trailing one or two are ignored).
std::vector<Triangle> trianglesOf(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);

// An xz bucket grid over upward-facing triangles, so a point on the ground
// can find the triangle under it and read back its height and smooth normal.
class TriangleGrid {
 public:
  // `cell` is the bucket size in metres.
  TriangleGrid(std::vector<Triangle> triangles, float cell);

  bool empty() const { return tris_.empty(); }
  // The triangles' corners (xz).
  glm::vec2 min() const { return min_; }
  glm::vec2 max() const { return max_; }

  struct Hit {
    glm::vec3 pos;
    glm::vec3 normal;
  };

  // The first triangle found under (x, z): the point on it and the normal
  // interpolated from its corners; nothing off the grid or between
  // triangles.
  std::optional<Hit> at(float x, float z) const;

 private:
  std::pair<int, int> cellOf(glm::vec2 p) const;

  std::vector<Triangle> tris_;
  std::vector<std::vector<std::size_t>> buckets_;
  float cell_;
  glm::vec2 min_{1e9f}, max_{-1e9f};
  int w_ = 1, h_ = 1;
};

}  // namespace pxe
