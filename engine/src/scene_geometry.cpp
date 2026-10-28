#include "pxe/scene_geometry.hpp"

#include <glm/common.hpp>

namespace pxe {

void SurfaceGeometry::append(const GltfPrimitive& primitive) {
  append(primitive.surface, primitive.vertices, primitive.indices);
}

void SurfaceGeometry::append(Surface surface, const std::vector<Vertex>& verts,
                             const std::vector<std::uint32_t>& idx) {
  auto& v = vertices[surface];
  auto& out = indices[surface];
  const auto base = static_cast<std::uint32_t>(v.size());
  v.insert(v.end(), verts.begin(), verts.end());
  for (std::uint32_t i : idx) out.push_back(base + i);
  for (const Vertex& p : verts) {
    boundsMin = glm::min(boundsMin, glm::vec3(p.px, p.py, p.pz));
    boundsMax = glm::max(boundsMax, glm::vec3(p.px, p.py, p.pz));
  }
}

std::vector<glm::vec3> SurfaceGeometry::corners(Surface surface) const {
  const auto& v = vertices[surface];
  const auto& idx = indices[surface];
  std::vector<glm::vec3> out;
  out.reserve(idx.size());
  for (std::uint32_t i : idx) out.push_back({v[i].px, v[i].py, v[i].pz});
  return out;
}

void buildSceneMaps(Scene& scene, const SurfaceGeometry& geometry,
                    const std::function<bool(glm::vec2, float)>& overhead) {
  {
    std::vector<glm::vec3> roofs = geometry.corners(kSurfStone);
    const std::vector<glm::vec3> slates = geometry.corners(kSurfRoof);
    roofs.insert(roofs.end(), slates.begin(), slates.end());
    scene.shelter.build(roofs);
  }
  std::vector<glm::vec3> solid;
  for (Surface s : {kSurfGrass, kSurfDirt, kSurfStone, kSurfBark, kSurfFoam, kSurfWood, kSurfPlaster, kSurfOak}) {
    const std::vector<glm::vec3> c = geometry.corners(s);
    solid.insert(solid.end(), c.begin(), c.end());
  }
  scene.ground.build(solid, 0.2f, overhead);
  scene.waterTop.build(geometry.corners(kSurfWater));
}

void uploadScene(Scene& scene, const SurfaceGeometry& geometry) {
  scene.boundsMin = glm::min(scene.boundsMin, geometry.boundsMin);
  scene.boundsMax = glm::max(scene.boundsMax, geometry.boundsMax);
  for (int s = 0; s < kSurfCount; ++s) {
    if (geometry.indices[s].empty()) continue;
    auto& target = (s == kSurfWater) ? scene.water : scene.meshes;
    target.emplace_back();
    target.back().upload(geometry.vertices[s], geometry.indices[s]);
  }
  const auto& water = geometry.vertices[kSurfWater];
  if (!water.empty()) {
    scene.waterMin = glm::vec3{1e9f};
    scene.waterMax = glm::vec3{-1e9f};
    for (const Vertex& v : water) {
      scene.waterMin = glm::min(scene.waterMin, glm::vec3(v.px, v.py, v.pz));
      scene.waterMax = glm::max(scene.waterMax, glm::vec3(v.px, v.py, v.pz));
    }
    scene.waterLevel = 0.5f * (scene.waterMin.y + scene.waterMax.y);
  }
}

}  // namespace pxe
