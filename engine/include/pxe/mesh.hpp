#pragma once

#include <cstdint>
#include <vector>

namespace pxe {

// Position only. Lighting reads a baked normal texture, not vertex normals,
// so band edges do not follow the triangulation.
struct TerrainVertex {
  float x, y, z;
};

class Mesh {
 public:
  Mesh() = default;
  ~Mesh();
  Mesh(Mesh&& other) noexcept;
  Mesh& operator=(Mesh&& other) noexcept;
  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  void upload(const std::vector<TerrainVertex>& vertices, const std::vector<std::uint32_t>& indices);
  void draw() const;
  bool empty() const { return indexCount_ == 0; }

 private:
  void destroy();
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
  unsigned ebo_ = 0;
  int indexCount_ = 0;
};

}  // namespace pxe
