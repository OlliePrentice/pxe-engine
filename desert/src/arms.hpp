#pragma once

#include "pxe/first_person_camera.hpp"
#include "pxe/renderer.hpp"
#include "pxe/skinned.hpp"

#include <glm/mat4x4.hpp>

#include <vector>

namespace desert {

// The player's arms (desert/assets/characters/arms.glb): a rigged model
// drawn as the view model in front of everything. Modelled round the eye, so
// the view's own transform places it. They are held in the rest pose while
// the player stands still, and walk with the player: two looping clips,
// Arms_Walk and Arms_Sprint, played by the distance the eye moves and blended
// in by its speed. The game owns it.
class Arms {
 public:
  // Actor-material slots 0-14: Arms_Wrap, Arms_Glove, Arms_Strap, Arms_Brass,
  // Arms_Glow, Arms_Steel, Arms_Copper, Arms_Rubber, Arms_Thread, Arms_Worn,
  // Arms_Grime, Arms_Dial, Arms_Paint, Arms_Amber, Arms_Screen, in the glb's
  // material order.
  static constexpr int kFirstMaterial = 0;

  // Needs the GL context. Throws std::runtime_error if the glb can't be read
  // or lacks the Arms_Walk or Arms_Sprint clip.
  void load();
  // Advances the walk by the ground distance the eye moved over the last
  // dt seconds (Player::moved, 0 while the player isn't moving).
  void walk(float moved, float dt);
  // Places the arms at `view`, walking or at rest, and adds them to the
  // frame: params.viewModel and their actor materials. Their joints stay
  // valid until the next pose().
  void pose(const pxe::PerspectiveFrame& view, pxe::FrameParams& params);

 private:
  pxe::SkinnedModel model_;
  pxe::SkinnedModel::Pose rest_, walkPose_, sprintPose_, pose_;
  std::vector<glm::mat4> joints_;
  int walkClip_ = -1;
  int sprintClip_ = -1;
  float phase_ = 0.f;      // 0..1 through the stride cycle
  float moving_ = 0.f;     // the walk's eased weight over rest, 0..1
  float sprinting_ = 0.f;  // the sprint's eased weight over the walk, 0..1
};

}  // namespace desert
