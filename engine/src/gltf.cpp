#include "pxe/gltf.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace pxe {
namespace {

float linearToSrgb(float c) {
  c = std::clamp(c, 0.f, 1.f);
  if (c <= 0.0031308f) return c * 12.92f;
  return 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
}

std::string lower(const char* text) {
  std::string out = text ? text : "";
  for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return out;
}

float surfaceForMaterial(const std::string& name) {
  if (name.find("leaf") != std::string::npos || name.find("canopy") != std::string::npos ||
      name.find("bush") != std::string::npos || name.find("foliage") != std::string::npos) {
    return static_cast<float>(kSurfLeaf);
  }
  if (name.find("bark") != std::string::npos || name.find("trunk") != std::string::npos ||
      name.find("wood") != std::string::npos) {
    return static_cast<float>(kSurfBark);
  }
  if (name.find("dirt") != std::string::npos || name.find("path") != std::string::npos) {
    return static_cast<float>(kSurfDirt);
  }
  if (name.find("grass") != std::string::npos || name.find("ground") != std::string::npos ||
      name.find("terrain") != std::string::npos || name.find("moss") != std::string::npos) {
    return static_cast<float>(kSurfGround);
  }
  return static_cast<float>(kSurfStone);
}

const cgltf_accessor* attribute(const cgltf_primitive* prim, cgltf_attribute_type type) {
  for (cgltf_size i = 0; i < prim->attributes_count; ++i) {
    if (prim->attributes[i].type == type) return prim->attributes[i].data;
  }
  return nullptr;
}

void readVec3(const cgltf_accessor* acc, cgltf_size index, float out[3]) {
  if (!acc || cgltf_accessor_read_float(acc, index, out, 3) == 0) {
    out[0] = out[1] = out[2] = 0.f;
  }
}

}  // namespace

GltfAsset loadGltf(const std::string& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("glTF not found: " + path);
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  cgltf_result parsed = cgltf_parse_file(&options, path.c_str(), &data);
  if (parsed != cgltf_result_success || !data) {
    throw std::runtime_error("failed to parse glTF: " + path);
  }
  cgltf_result buffers = cgltf_load_buffers(&options, data, path.c_str());
  if (buffers != cgltf_result_success) {
    cgltf_free(data);
    throw std::runtime_error("failed to load glTF buffers: " + path);
  }

  GltfAsset asset;
  asset.nodes.reserve(data->nodes_count);
  for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
    const cgltf_node* node = &data->nodes[ni];
    if (!node->mesh) continue;

    GltfNode out;
    out.name = node->name ? node->name : "";
    cgltf_float matrix[16];
    cgltf_node_transform_world(node, matrix);
    out.transform = glm::make_mat4(matrix);

    for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi) {
      const cgltf_primitive* prim = &node->mesh->primitives[pi];
      if (prim->type != cgltf_primitive_type_triangles) {
        cgltf_free(data);
        throw std::runtime_error("glTF primitive is not triangles (export with triangulation): " + path);
      }
      const cgltf_accessor* pos = attribute(prim, cgltf_attribute_type_position);
      if (!pos) continue;
      const cgltf_accessor* nrm = attribute(prim, cgltf_attribute_type_normal);
      const cgltf_accessor* col = attribute(prim, cgltf_attribute_type_color);

      const cgltf_material* mat = prim->material;
      const char* matName = (mat && mat->name) ? mat->name : "Stone";
      float cr = 1.f, cg = 1.f, cb = 1.f;
      if (mat && mat->has_pbr_metallic_roughness) {
        const float* f = mat->pbr_metallic_roughness.base_color_factor;
        cr = linearToSrgb(f[0]);
        cg = linearToSrgb(f[1]);
        cb = linearToSrgb(f[2]);
      }

      GltfPrimitive gp;
      gp.material = matName;
      gp.surface = surfaceForMaterial(lower(matName));
      gp.vertices.resize(pos->count);
      for (cgltf_size vi = 0; vi < pos->count; ++vi) {
        float p[3], n[3]{0.f, 1.f, 0.f}, c[4]{1.f, 1.f, 1.f, 1.f};
        readVec3(pos, vi, p);
        if (nrm) readVec3(nrm, vi, n);
        if (col) cgltf_accessor_read_float(col, vi, c, 4);
        Vertex v{};
        v.px = p[0];
        v.py = p[1];
        v.pz = p[2];
        v.nx = n[0];
        v.ny = n[1];
        v.nz = n[2];
        v.cr = cr * c[0];
        v.cg = cg * c[1];
        v.cb = cb * c[2];
        v.surface = gp.surface;
        gp.vertices[vi] = v;
      }

      if (prim->indices) {
        gp.indices.resize(prim->indices->count);
        for (cgltf_size ii = 0; ii < prim->indices->count; ++ii) {
          gp.indices[ii] = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim->indices, ii));
        }
      } else {
        gp.indices.resize(pos->count);
        for (cgltf_size ii = 0; ii < pos->count; ++ii) gp.indices[ii] = static_cast<std::uint32_t>(ii);
      }

      if (!nrm) {
        for (std::size_t i = 0; i + 2 < gp.indices.size(); i += 3) {
          Vertex& a = gp.vertices[gp.indices[i]];
          Vertex& b = gp.vertices[gp.indices[i + 1]];
          Vertex& c = gp.vertices[gp.indices[i + 2]];
          const glm::vec3 fn = glm::normalize(glm::cross(glm::vec3{b.px - a.px, b.py - a.py, b.pz - a.pz},
                                                         glm::vec3{c.px - a.px, c.py - a.py, c.pz - a.pz}));
          a.nx = b.nx = c.nx = fn.x;
          a.ny = b.ny = c.ny = fn.y;
          a.nz = b.nz = c.nz = fn.z;
        }
      }

      out.primitives.push_back(std::move(gp));
    }
    if (!out.primitives.empty()) asset.nodes.push_back(std::move(out));
  }

  cgltf_free(data);
  if (asset.nodes.empty()) throw std::runtime_error("glTF has no mesh nodes: " + path);
  return asset;
}

}  // namespace pxe
