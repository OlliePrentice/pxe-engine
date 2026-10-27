#include "pxe/gltf.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace pxe {
namespace {

Surface surfaceFor(const char* name) {
  std::string s = name ? name : "";
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  // Blender appends ".001" to duplicate names; match on the prefix.
  auto has = [&](const char* key) { return s.rfind(key, 0) == 0; };
  if (has("grass")) return kSurfGrass;
  if (has("dirt")) return kSurfDirt;
  if (has("bark")) return kSurfBark;
  if (has("leaf")) return kSurfLeaf;
  if (has("water")) return kSurfWater;
  if (has("foam")) return kSurfFoam;
  if (has("wood")) return kSurfWood;
  if (has("plaster")) return kSurfPlaster;
  if (has("roof")) return kSurfRoof;
  if (has("oak")) return kSurfOak;
  return kSurfStone;
}

const cgltf_accessor* attribute(const cgltf_primitive& prim, cgltf_attribute_type type) {
  for (cgltf_size i = 0; i < prim.attributes_count; ++i) {
    if (prim.attributes[i].type == type) return prim.attributes[i].data;
  }
  return nullptr;
}

}  // namespace

std::vector<GltfPrimitive> loadGltf(const std::string& path) {
  if (!std::filesystem::exists(path)) throw std::runtime_error("glTF not found: " + path);

  cgltf_options options{};
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success || !data) {
    throw std::runtime_error("failed to parse glTF: " + path);
  }
  if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
    cgltf_free(data);
    throw std::runtime_error("failed to load glTF buffers: " + path);
  }

  std::vector<GltfPrimitive> out;
  for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
    const cgltf_node& node = data->nodes[ni];
    if (!node.mesh) continue;
    cgltf_float world[16];
    cgltf_node_transform_world(&node, world);
    const glm::mat4 m = glm::make_mat4(world);
    const glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(m)));

    for (cgltf_size pi = 0; pi < node.mesh->primitives_count; ++pi) {
      const cgltf_primitive& prim = node.mesh->primitives[pi];
      if (prim.type != cgltf_primitive_type_triangles) continue;
      const cgltf_accessor* pos = attribute(prim, cgltf_attribute_type_position);
      const cgltf_accessor* nrm = attribute(prim, cgltf_attribute_type_normal);
      if (!pos || !nrm) continue;

      GltfPrimitive gp;
      gp.node = node.name ? node.name : "";
      gp.material = (prim.material && prim.material->name) ? prim.material->name : "";
      gp.surface = surfaceFor(gp.material.c_str());
      gp.vertices.resize(pos->count);
      for (cgltf_size vi = 0; vi < pos->count; ++vi) {
        float p[3]{}, n[3]{0.f, 1.f, 0.f};
        cgltf_accessor_read_float(pos, vi, p, 3);
        cgltf_accessor_read_float(nrm, vi, n, 3);
        const glm::vec3 wp = glm::vec3(m * glm::vec4(p[0], p[1], p[2], 1.f));
        const glm::vec3 wn = glm::normalize(nm * glm::vec3(n[0], n[1], n[2]));
        gp.vertices[vi] = {wp.x, wp.y, wp.z, wn.x, wn.y, wn.z, static_cast<float>(gp.surface)};
      }
      if (prim.indices) {
        gp.indices.resize(prim.indices->count);
        for (cgltf_size ii = 0; ii < prim.indices->count; ++ii)
          gp.indices[ii] = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, ii));
      } else {
        gp.indices.resize(pos->count);
        for (cgltf_size ii = 0; ii < pos->count; ++ii) gp.indices[ii] = static_cast<std::uint32_t>(ii);
      }
      out.push_back(std::move(gp));
    }
  }
  cgltf_free(data);
  if (out.empty()) throw std::runtime_error("glTF has no triangle meshes: " + path);
  return out;
}

}  // namespace pxe
