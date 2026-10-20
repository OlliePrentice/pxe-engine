#pragma once

#include "pxe/mesh.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace pxe {

// Characters are drawn as a surface of their own: every vertex's surface is
// kActorSurface + its material's index in the model, and FrameParams::actorMaterials
// gives each index its base colour and whether it glows.
constexpr int kActorSurface = 16;
constexpr int kActorMaterials = 8;

struct ActorMaterial {
  glm::vec3 colour{0.f};  // base colour, display (sRGB) values
  bool glow = false;      // drawn at full colour, unlit
};

// A skinned character from glTF 2.0: one skinned mesh, its skeleton and its
// animation clips. Clips are sampled at any time, with joints interpolated
// between keys (slerped rotations), and poses blend, so motion is smooth
// rather than stepped. Skinning runs on the CPU straight into world-space
// vertices, so a posed character draws through every pass - shadows,
// reflections, edges - exactly as the scene's own meshes do.
class SkinnedModel {
 public:
  // Each joint's local transform.
  struct Pose {
    std::vector<glm::vec3> t;
    std::vector<glm::quat> r;
    std::vector<glm::vec3> s;
  };

  // Throws std::runtime_error on a missing file or one with no skinned mesh.
  static SkinnedModel load(const std::string& path);

  int jointCount() const { return static_cast<int>(joints_.size()); }
  int joint(const std::string& name) const;  // -1 when absent
  int clip(const std::string& name) const;   // -1 when absent
  float clipDuration(int clip) const;
  const std::vector<ActorMaterial>& materials() const { return materials_; }

  void restPose(Pose& pose) const;
  // One clip at `time` seconds over the rest pose: wrapped round its length
  // when it loops, held at its ends when it plays once.
  void sample(int clip, float time, Pose& pose, bool loop = true) const;
  // a toward b by w (0 all a .. 1 all b), into out (which may be a or b).
  static void blend(const Pose& a, const Pose& b, float w, Pose& out);
  // The same with a weight for each joint, for layering one action over
  // another on part of the body.
  static void blend(const Pose& a, const Pose& b, const std::vector<float>& w, Pose& out);
  // Turn a joint by `angle` radians about `axis`, an axis of the model at
  // rest (y up, facing +z), after its animation; its children follow.
  void turn(Pose& pose, int joint, glm::vec3 axis, float angle) const;
  // Skin a pose placed by `model` into world-space vertices (same count and
  // order every call), surfaces kActorSurface + material.
  void skin(const Pose& pose, const glm::mat4& model, std::vector<Vertex>& out) const;
  const std::vector<std::uint32_t>& indices() const { return indices_; }
  // A joint's place and axes at rest, in the model (y up, facing +z).
  glm::mat4 restJoint(int joint) const;
  // Where a point of the model at rest, carried by `joint`, is in the world
  // as of the last skin().
  glm::vec3 jointPoint(int joint, glm::vec3 restPoint) const;

 private:
  struct Joint {
    std::string name;
    int parent = -1;  // always earlier in the list
    glm::vec3 t{0.f};
    glm::quat r{1.f, 0.f, 0.f, 0.f};
    glm::vec3 s{1.f};
    glm::mat4 inverseBind{1.f};
    glm::quat restWorld{1.f, 0.f, 0.f, 0.f};  // rotation at rest, in the model
  };
  struct Channel {
    int joint = 0;
    int path = 0;  // 0 translation, 1 rotation, 2 scale
    bool step = false;
    std::vector<float> times;
    std::vector<glm::vec4> values;
  };
  struct Clip {
    std::string name;
    float duration = 0.f;
    std::vector<Channel> channels;
  };
  struct SkinVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    float surface;
    int joints[4];
    float weights[4];
  };

  std::vector<Joint> joints_;
  std::vector<Clip> clips_;
  std::vector<SkinVertex> vertices_;
  std::vector<std::uint32_t> indices_;
  std::vector<ActorMaterial> materials_;
  glm::mat4 base_{1.f};  // world transform of the skeleton's parent node
  mutable std::vector<glm::mat4> world_, skinMats_;
};

}  // namespace pxe
