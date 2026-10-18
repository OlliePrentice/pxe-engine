#pragma once

#include <cstdint>
#include <vector>

namespace pxe {

// Material families. Each one has its own three-step toon ramp, and the
// glTF loader picks one from the material name.
enum Surface : int {
  kSurfGrass = 0,
  kSurfDirt = 1,
  kSurfStone = 2,
  kSurfBark = 3,
  kSurfLeaf = 4,
  kSurfWater = 5,
  kSurfFoam = 6,
  kSurfCount = 7,
};

// World-space vertex. Lighting uses the vertex normal, so a crown's custom
// normals from Blender survive into the toon ramp.
struct Vertex {
  float px, py, pz;
  float nx, ny, nz;
  float surface;
};

class Mesh {
 public:
  Mesh() = default;
  ~Mesh();
  Mesh(Mesh&& other) noexcept;
  Mesh& operator=(Mesh&& other) noexcept;
  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  void upload(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
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
