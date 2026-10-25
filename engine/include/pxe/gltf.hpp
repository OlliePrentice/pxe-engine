#pragma once

#include "pxe/mesh.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pxe {

// One glTF primitive, already moved into world space (node transforms and
// parents applied, normals by the inverse transpose). `surface` comes from
// the material name: Grass, Dirt, Stone, Bark, Leaf, Water, Foam, Wood,
// Plaster or Roof (any other name is Stone).
struct GltfPrimitive {
  std::string node;
  std::string material;
  Surface surface = kSurfStone;
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
};

// glTF 2.0 (.glb or .gltf): triangle meshes, custom normals, node
// transforms and material names. Textures, skinning and animation are
// ignored. Throws std::runtime_error on a missing or unreadable file.
std::vector<GltfPrimitive> loadGltf(const std::string& path);

}  // namespace pxe
