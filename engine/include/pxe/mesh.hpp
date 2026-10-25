#pragma once

#include <cstdint>
#include <vector>

namespace pxe {

// Material families. Each one has its own three-step toon ramp, and the
// glTF loader picks one from the material name. Wood, plaster and roof are
// built structure (buildings and their furniture): flat-faced like cut
// stone, and outlined and rimmed in the edge pass as stone is.
enum Surface : int {
  kSurfGrass = 0,
  kSurfDirt = 1,
  kSurfStone = 2,
  kSurfBark = 3,
  kSurfLeaf = 4,
  kSurfWater = 5,
  kSurfFoam = 6,
  kSurfWood = 7,
  kSurfPlaster = 8,
  kSurfRoof = 9,
  kSurfCount = 10,
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
  // For a mesh rebuilt every frame (a sword's smear): the first upload makes
  // the buffers, later ones rewrite the vertices in place. The vertex count
  // and indices must not change.
  void update(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
  void draw() const;
  bool empty() const { return indexCount_ == 0; }

 private:
  void destroy();
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
  unsigned ebo_ = 0;
  int indexCount_ = 0;
};

// A skinned character's vertex at rest, in its model: the vertex shaders
// place it by up to four of the model's joints, whose weights sum to 1.
struct SkinVertex {
  float px, py, pz;
  float nx, ny, nz;
  float surface;
  std::uint8_t joints[4];
  float weights[4];
};

// A skinned character's mesh, uploaded once at rest. Every character drawn
// from it hands the renderer its own joint matrices (FrameParams::actors),
// so one upload serves any number of them.
class SkinnedMesh {
 public:
  SkinnedMesh() = default;
  ~SkinnedMesh();
  SkinnedMesh(SkinnedMesh&& other) noexcept;
  SkinnedMesh& operator=(SkinnedMesh&& other) noexcept;
  SkinnedMesh(const SkinnedMesh&) = delete;
  SkinnedMesh& operator=(const SkinnedMesh&) = delete;

  void upload(const std::vector<SkinVertex>& vertices, const std::vector<std::uint32_t>& indices);
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
