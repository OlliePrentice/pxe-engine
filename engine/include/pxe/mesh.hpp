#pragma once

#include <cstdint>
#include <vector>

namespace pxe {

// Tightly packed so the layout does not depend on glm's alignment settings.
struct Vertex {
  float px, py, pz;
  float nx, ny, nz;
  float cr, cg, cb;
  float surface;
};

struct FoliageInstance {
  float px, py, pz;
  float nx, ny, nz;
  float cr, cg, cb;
  float phase;
  float scale;
  float kind;  // 0 grass, 1 violet flower, 2 yellow flower, 3 leaf
  float pad;
};

static_assert(sizeof(Vertex) == sizeof(float) * 10, "Vertex layout");
static_assert(sizeof(FoliageInstance) == sizeof(float) * 13, "Instance layout");

enum Surface : int {
  kSurfGround = 0,
  kSurfDirt = 1,
  kSurfStone = 2,
  kSurfBark = 3,
  kSurfLeaf = 4,
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
  int indexCount() const { return indexCount_; }

 private:
  void destroy();
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
  unsigned ebo_ = 0;
  int indexCount_ = 0;
};

}  // namespace pxe
