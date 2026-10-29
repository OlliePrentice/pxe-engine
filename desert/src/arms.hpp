#pragma once

#include "pxe/first_person_camera.hpp"
#include "pxe/renderer.hpp"
#include "pxe/skinned.hpp"

#include <glm/mat4x4.hpp>

#include <vector>

namespace desert {

// The player's arms (desert/assets/characters/arms.glb): a rigged model
// held in its rest pose, which is the held pose, drawn as the view model
// in front of everything. Modelled round the eye, so the view's own
// transform places it. The game owns it.
class Arms {
 public:
  // Actor-material slots 0-4: Arms_Wrap, Arms_Glove, Arms_Strap, Arms_Brass,
  // Arms_Glow, in the glb's material order.
  static constexpr int kFirstMaterial = 0;

  // Needs the GL context. Throws std::runtime_error if the glb can't be read.
  void load();
  // Places the arms at `view` and adds them to the frame: params.viewModel
  // and their actor materials. Their joints stay valid until the next
  // pose().
  void pose(const pxe::PerspectiveFrame& view, pxe::FrameParams& params);

 private:
  pxe::SkinnedModel model_;
  pxe::SkinnedModel::Pose rest_;
  std::vector<glm::mat4> joints_;
};

}  // namespace desert
