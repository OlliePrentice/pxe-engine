#pragma once

#include "pxe/mesh.hpp"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace pxe {

// One glTF primitive. Vertices are in the node's local space. `surface` is
// chosen from the material name (Leaf / Bark / Stone / ...). Vertex colour
// is the material base colour converted from glTF linear to sRGB, because
// the toon pass treats albedo as a display colour.
struct GltfPrimitive {
  std::string material;
  float surface = kSurfStone;
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
};

struct GltfNode {
  std::string name;
  glm::mat4 transform{1.f};  // world, parents included, column-major
  std::vector<GltfPrimitive> primitives;
};

struct GltfAsset {
  std::vector<GltfNode> nodes;
};

// glTF 2.0 (.glb or .gltf). Meshes, custom normals, per-material base
// colour, and node transforms. Textures, skinning, and animation are ignored.
// Throws std::runtime_error on a missing file or a mesh that is not triangles.
GltfAsset loadGltf(const std::string& path);

}  // namespace pxe
