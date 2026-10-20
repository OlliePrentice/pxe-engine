#include "pxe/skinned.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "cgltf.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace pxe {
namespace {

const cgltf_accessor* attribute(const cgltf_primitive& prim, cgltf_attribute_type type) {
  for (cgltf_size i = 0; i < prim.attributes_count; ++i)
    if (prim.attributes[i].type == type) return prim.attributes[i].data;
  return nullptr;
}

float toSrgb(float c) {
  c = std::clamp(c, 0.f, 1.f);
  return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
}

glm::mat4 trs(glm::vec3 t, glm::quat r, glm::vec3 s) {
  return glm::translate(glm::mat4{1.f}, t) * glm::mat4_cast(r) * glm::scale(glm::mat4{1.f}, s);
}

}  // namespace

SkinnedModel SkinnedModel::load(const std::string& path) {
  if (!std::filesystem::exists(path)) throw std::runtime_error("model not found: " + path);
  cgltf_options options{};
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success || !data)
    throw std::runtime_error("failed to parse glTF: " + path);
  struct Free {
    cgltf_data* d;
    ~Free() { cgltf_free(d); }
  } guard{data};
  if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success)
    throw std::runtime_error("failed to load glTF buffers: " + path);

  const cgltf_node* meshNode = nullptr;
  for (cgltf_size i = 0; i < data->nodes_count && !meshNode; ++i)
    if (data->nodes[i].mesh && data->nodes[i].skin) meshNode = &data->nodes[i];
  if (!meshNode) throw std::runtime_error("no skinned mesh in " + path);
  const cgltf_skin& skin = *meshNode->skin;

  SkinnedModel m;
  // Joints, parents first: sort the skin's joints by depth, and keep a map
  // from the skin's order (which the vertices use) to ours.
  auto depth = [](const cgltf_node* n) {
    int d = 0;
    for (; n->parent; n = n->parent) ++d;
    return d;
  };
  std::vector<int> order(skin.joints_count);
  for (cgltf_size i = 0; i < skin.joints_count; ++i) order[i] = static_cast<int>(i);
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return depth(skin.joints[a]) < depth(skin.joints[b]); });
  std::vector<int> fromSkin(skin.joints_count);
  for (cgltf_size k = 0; k < order.size(); ++k) fromSkin[order[k]] = static_cast<int>(k);
  auto indexOf = [&](const cgltf_node* n) {
    for (cgltf_size i = 0; i < skin.joints_count; ++i)
      if (skin.joints[i] == n) return fromSkin[i];
    return -1;
  };
  m.joints_.resize(skin.joints_count);
  for (cgltf_size k = 0; k < order.size(); ++k) {
    const cgltf_node* n = skin.joints[order[k]];
    Joint& j = m.joints_[k];
    j.name = n->name ? n->name : "";
    j.parent = n->parent ? indexOf(n->parent) : -1;
    if (n->has_translation) j.t = glm::make_vec3(n->translation);
    if (n->has_rotation) j.r = glm::quat(n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]);
    if (n->has_scale) j.s = glm::make_vec3(n->scale);
    if (skin.inverse_bind_matrices) {
      float mat[16];
      cgltf_accessor_read_float(skin.inverse_bind_matrices, static_cast<cgltf_size>(order[k]), mat, 16);
      j.inverseBind = glm::make_mat4(mat);
    }
  }
  // Whatever the skeleton hangs from (the armature's node) places it all.
  if (m.joints_[0].parent < 0 && skin.joints[order[0]]->parent) {
    float mat[16];
    cgltf_node_transform_world(skin.joints[order[0]]->parent, mat);
    m.base_ = glm::make_mat4(mat);
  }
  for (std::size_t k = 0; k < m.joints_.size(); ++k) {
    Joint& j = m.joints_[k];
    const glm::quat up = j.parent >= 0 ? m.joints_[j.parent].restWorld : glm::quat_cast(glm::mat3(m.base_));
    j.restWorld = glm::normalize(up * j.r);
  }

  // Materials: base colour to display values; anything emissive glows.
  for (cgltf_size i = 0; i < data->materials_count && i < kActorMaterials; ++i) {
    const cgltf_material& mat = data->materials[i];
    ActorMaterial a;
    const float* c = mat.has_pbr_metallic_roughness ? mat.pbr_metallic_roughness.base_color_factor : nullptr;
    if (c) a.colour = {toSrgb(c[0]), toSrgb(c[1]), toSrgb(c[2])};
    const float* e = mat.emissive_factor;
    a.glow = (e[0] + e[1] + e[2]) > 0.01f;
    m.materials_.push_back(a);
  }

  // Vertices, every primitive of the mesh.
  for (cgltf_size p = 0; p < meshNode->mesh->primitives_count; ++p) {
    const cgltf_primitive& prim = meshNode->mesh->primitives[p];
    const cgltf_accessor* pos = attribute(prim, cgltf_attribute_type_position);
    const cgltf_accessor* nrm = attribute(prim, cgltf_attribute_type_normal);
    const cgltf_accessor* jts = attribute(prim, cgltf_attribute_type_joints);
    const cgltf_accessor* wts = attribute(prim, cgltf_attribute_type_weights);
    if (!pos || !nrm || !jts || !wts) continue;
    const int material = prim.material ? static_cast<int>(prim.material - data->materials) : 0;
    const auto first = static_cast<std::uint32_t>(m.vertices_.size());
    for (cgltf_size i = 0; i < pos->count; ++i) {
      SkinVertex v{};
      cgltf_accessor_read_float(pos, i, glm::value_ptr(v.pos), 3);
      cgltf_accessor_read_float(nrm, i, glm::value_ptr(v.normal), 3);
      v.surface = static_cast<float>(kActorSurface + std::min(material, kActorMaterials - 1));
      cgltf_uint ji[4] = {};
      cgltf_accessor_read_uint(jts, i, ji, 4);
      cgltf_accessor_read_float(wts, i, v.weights, 4);
      float total = 0.f;
      for (int k = 0; k < 4; ++k) {
        v.joints[k] = ji[k] < fromSkin.size() ? fromSkin[ji[k]] : 0;
        total += v.weights[k];
      }
      for (float& w : v.weights) w = total > 0.f ? w / total : 0.f;
      m.vertices_.push_back(v);
    }
    if (prim.indices) {
      for (cgltf_size i = 0; i < prim.indices->count; ++i)
        m.indices_.push_back(first + static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, i)));
    } else {
      for (cgltf_size i = 0; i < pos->count; ++i) m.indices_.push_back(first + static_cast<std::uint32_t>(i));
    }
  }
  if (m.vertices_.empty()) throw std::runtime_error("skinned mesh has no vertices: " + path);

  // Clips: every channel that drives one of the joints.
  for (cgltf_size a = 0; a < data->animations_count; ++a) {
    const cgltf_animation& anim = data->animations[a];
    Clip clip;
    clip.name = anim.name ? anim.name : "";
    for (cgltf_size c = 0; c < anim.channels_count; ++c) {
      const cgltf_animation_channel& ch = anim.channels[c];
      const int joint = indexOf(ch.target_node);
      if (joint < 0 || !ch.sampler) continue;
      Channel out;
      out.joint = joint;
      if (ch.target_path == cgltf_animation_path_type_translation) out.path = 0;
      else if (ch.target_path == cgltf_animation_path_type_rotation) out.path = 1;
      else if (ch.target_path == cgltf_animation_path_type_scale) out.path = 2;
      else continue;
      const cgltf_animation_sampler& s = *ch.sampler;
      out.step = s.interpolation == cgltf_interpolation_type_step;
      const bool cubic = s.interpolation == cgltf_interpolation_type_cubic_spline;
      const int width = out.path == 1 ? 4 : 3;
      out.times.resize(s.input->count);
      for (cgltf_size k = 0; k < s.input->count; ++k) cgltf_accessor_read_float(s.input, k, &out.times[k], 1);
      for (cgltf_size k = 0; k < s.input->count; ++k) {
        glm::vec4 v{0.f};
        cgltf_accessor_read_float(s.output, cubic ? k * 3 + 1 : k, glm::value_ptr(v), width);
        out.values.push_back(v);
      }
      if (!out.times.empty()) clip.duration = std::max(clip.duration, out.times.back());
      clip.channels.push_back(std::move(out));
    }
    m.clips_.push_back(std::move(clip));
  }
  return m;
}

int SkinnedModel::joint(const std::string& name) const {
  for (std::size_t i = 0; i < joints_.size(); ++i)
    if (joints_[i].name == name) return static_cast<int>(i);
  return -1;
}

int SkinnedModel::clip(const std::string& name) const {
  for (std::size_t i = 0; i < clips_.size(); ++i)
    if (clips_[i].name == name) return static_cast<int>(i);
  return -1;
}

float SkinnedModel::clipDuration(int clip) const {
  return clip >= 0 && clip < static_cast<int>(clips_.size()) ? clips_[clip].duration : 0.f;
}

void SkinnedModel::restPose(Pose& pose) const {
  const std::size_t n = joints_.size();
  pose.t.resize(n);
  pose.r.resize(n);
  pose.s.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    pose.t[i] = joints_[i].t;
    pose.r[i] = joints_[i].r;
    pose.s[i] = joints_[i].s;
  }
}

void SkinnedModel::sample(int clip, float time, Pose& pose) const {
  if (pose.r.size() != joints_.size()) restPose(pose);
  if (clip < 0 || clip >= static_cast<int>(clips_.size())) return;
  const Clip& c = clips_[clip];
  if (c.duration > 0.f) {
    time = std::fmod(time, c.duration);
    if (time < 0.f) time += c.duration;
  }
  for (const Channel& ch : c.channels) {
    const std::vector<float>& ts = ch.times;
    glm::vec4 v = ch.values.front();
    if (ts.size() > 1 && time > ts.front()) {
      const auto it = std::upper_bound(ts.begin(), ts.end(), time);
      if (it == ts.end()) {
        v = ch.values.back();
      } else {
        const std::size_t k = static_cast<std::size_t>(it - ts.begin());
        const float f = ch.step ? 0.f : (time - ts[k - 1]) / std::max(ts[k] - ts[k - 1], 1e-6f);
        const glm::vec4& a = ch.values[k - 1];
        const glm::vec4& b = ch.values[k];
        if (ch.path == 1) {
          const glm::quat qa(a.w, a.x, a.y, a.z), qb(b.w, b.x, b.y, b.z);
          const glm::quat q = glm::slerp(qa, glm::dot(qa, qb) < 0.f ? -qb : qb, f);
          v = {q.x, q.y, q.z, q.w};
        } else {
          v = glm::mix(a, b, f);
        }
      }
    }
    if (ch.path == 0) pose.t[ch.joint] = glm::vec3(v);
    else if (ch.path == 1) pose.r[ch.joint] = glm::normalize(glm::quat(v.w, v.x, v.y, v.z));
    else pose.s[ch.joint] = glm::vec3(v);
  }
}

void SkinnedModel::blend(const Pose& a, const Pose& b, float w, Pose& out) {
  const std::size_t n = std::min(a.r.size(), b.r.size());
  out.t.resize(n);
  out.r.resize(n);
  out.s.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    out.t[i] = glm::mix(a.t[i], b.t[i], w);
    const glm::quat qb = glm::dot(a.r[i], b.r[i]) < 0.f ? -b.r[i] : b.r[i];
    out.r[i] = glm::normalize(glm::slerp(a.r[i], qb, w));
    out.s[i] = glm::mix(a.s[i], b.s[i], w);
  }
}

void SkinnedModel::turn(Pose& pose, int joint, glm::vec3 axis, float angle) const {
  if (joint < 0 || joint >= static_cast<int>(joints_.size()) || std::fabs(angle) < 1e-6f) return;
  const glm::vec3 local = glm::normalize(glm::inverse(joints_[joint].restWorld) * axis);
  pose.r[joint] = glm::normalize(pose.r[joint] * glm::angleAxis(angle, local));
}

void SkinnedModel::skin(const Pose& pose, const glm::mat4& model, std::vector<Vertex>& out) const {
  const std::size_t n = joints_.size();
  world_.resize(n);
  skinMats_.resize(n);
  const glm::mat4 root = model * base_;
  for (std::size_t i = 0; i < n; ++i) {
    const glm::mat4 local = trs(pose.t[i], pose.r[i], pose.s[i]);
    world_[i] = joints_[i].parent >= 0 ? world_[joints_[i].parent] * local : root * local;
    skinMats_[i] = world_[i] * joints_[i].inverseBind;
  }
  out.resize(vertices_.size());
  for (std::size_t i = 0; i < vertices_.size(); ++i) {
    const SkinVertex& v = vertices_[i];
    glm::mat4 m{0.f};
    for (int k = 0; k < 4; ++k)
      if (v.weights[k] > 0.f) m += skinMats_[v.joints[k]] * v.weights[k];
    const glm::vec3 p = glm::vec3(m * glm::vec4(v.pos, 1.f));
    const glm::vec3 nrm = glm::normalize(glm::mat3(m) * v.normal);
    out[i] = {p.x, p.y, p.z, nrm.x, nrm.y, nrm.z, v.surface};
  }
}

}  // namespace pxe
