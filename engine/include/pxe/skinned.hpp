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
// kActorSurface + an actor-material slot, and FrameParams::actorMaterials
// gives each slot its base colour and whether it glows. The characters share
// the table each frame, each kind in slots of its own: a model's materials
// take the slots from the first it was loaded with (SkinnedModel::load), and
// every character drawn from one model shares them.
constexpr int kActorSurface = 16;
constexpr int kActorMaterials = 64;
// The most joints a model may have: the vertex shaders take a character's
// joint matrices as a uniform array this long (uJoints).
constexpr int kMaxJoints = 64;

struct ActorMaterial {
  glm::vec3 colour{0.f};  // base colour, display (sRGB) values
  bool glow = false;      // drawn at full colour, unlit
  // Inked along its sharp creases in the edge pass, as slate is outdoors
  // (a moving prop's carved wood: the tavern door's planks and grain).
  // Outlined and rimmed as every actor is, either way.
  bool crease = false;
};

// A skinned character from glTF 2.0: one skinned mesh, its skeleton and its
// animation clips. Clips are sampled at any time, with joints interpolated
// between keys (slerped rotations), and poses blend, so motion is smooth
// rather than stepped. Skinning runs in the vertex shaders: the mesh is
// uploaded once, at rest, and a posed character is drawn from it with its
// own joint matrices (skin), through every pass - shadows, reflections,
// edges - as the scene's own meshes are. A model keeps nothing of any one
// character, so any number of them can share it.
class SkinnedModel {
 public:
  // Each joint's local transform.
  struct Pose {
    std::vector<glm::vec3> t;
    std::vector<glm::quat> r;
    std::vector<glm::vec3> s;
  };

  // Needs the GL context, to upload the mesh. Throws std::runtime_error on a
  // missing file, one with no skinned mesh or more than kMaxJoints joints.
  // The model's material i takes actor-material slot firstMaterial + i
  // (0 <= firstMaterial < kActorMaterials; materials past the table's end
  // share its last slot).
  static SkinnedModel load(const std::string& path, int firstMaterial = 0);

  int jointCount() const { return static_cast<int>(joints_.size()); }
  int joint(const std::string& name) const;  // -1 when absent
  int clip(const std::string& name) const;   // -1 when absent
  float clipDuration(int clip) const;
  // The model's materials, in glTF order; materials()[i] belongs in actor
  // slot firstMaterial() + i.
  const std::vector<ActorMaterial>& materials() const { return materials_; }
  int firstMaterial() const { return firstMaterial_; }
  // The mesh at rest, its vertices' surfaces kActorSurface + firstMaterial()
  // + material, to draw with a character's joint matrices.
  const SkinnedMesh& mesh() const { return mesh_; }

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
  // A pose placed by `model`: each joint's matrix from the model at rest to
  // the world (its world transform after its inverse bind), for drawing
  // (FrameParams::actors) and jointPoint.
  void skin(const Pose& pose, const glm::mat4& model, std::vector<glm::mat4>& joints) const;
  // A joint's place and axes at rest, in the model (y up, facing +z).
  glm::mat4 restJoint(int joint) const;
  // Where a point of the model at rest, carried by `joint`, is under a
  // character's joint matrices (skin).
  static glm::vec3 jointPoint(const std::vector<glm::mat4>& joints, int joint, glm::vec3 restPoint);
  // For measuring, not drawing: where every vertex of the mesh is under
  // joint matrices from skin(), in the mesh's order.
  void vertexPoints(const std::vector<glm::mat4>& joints, std::vector<glm::vec3>& out) const;

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
  std::vector<Joint> joints_;
  std::vector<Clip> clips_;
  std::vector<SkinVertex> vertices_;  // as uploaded, for vertexPoints
  std::vector<ActorMaterial> materials_;
  int firstMaterial_ = 0;
  glm::mat4 base_{1.f};  // world transform of the skeleton's parent node
  SkinnedMesh mesh_;
};

}  // namespace pxe
