#include "arms.hpp"

#include <glm/vec4.hpp>

#include <cstddef>
#include <string>

namespace desert {
namespace {

// The glb's materials, as offsets from Arms::kFirstMaterial.
constexpr int kWrap = 0;
constexpr int kGlove = 1;
[[maybe_unused]] constexpr int kStrap = 2;
constexpr int kBrass = 3;
[[maybe_unused]] constexpr int kGlow = 4;
[[maybe_unused]] constexpr int kSteel = 5;
[[maybe_unused]] constexpr int kCopper = 6;
[[maybe_unused]] constexpr int kRubber = 7;
[[maybe_unused]] constexpr int kThread = 8;
constexpr int kWorn = 9;
constexpr int kGrime = 10;
[[maybe_unused]] constexpr int kDial = 11;
[[maybe_unused]] constexpr int kPaint = 12;
[[maybe_unused]] constexpr int kAmber = 13;

}  // namespace

void Arms::load() {
  model_ = pxe::SkinnedModel::load(std::string(PXE_ASSET_DIR) + "/characters/arms.glb", kFirstMaterial);
  model_.restPose(rest_);
}

void Arms::pose(const pxe::PerspectiveFrame& view, pxe::FrameParams& params) {
  // The model faces +Z with +X on its left and +Y up, the way every
  // character faces at rest, so its axes map onto the view's: +Z forward,
  // +X the view's left, +Y up. The origin is the eye.
  glm::mat4 eye{1.f};
  eye[0] = glm::vec4(-view.right, 0.f);
  eye[1] = glm::vec4(view.up, 0.f);
  eye[2] = glm::vec4(view.forward, 0.f);
  eye[3] = glm::vec4(view.eye, 1.f);
  model_.skin(rest_, eye, joints_);

  params.viewModel.push_back({&model_.mesh(), joints_});
  const auto& mats = model_.materials();
  for (int i = 0; i < static_cast<int>(mats.size()) && kFirstMaterial + i < pxe::kActorMaterials; ++i) {
    params.actorMaterials[kFirstMaterial + i] = mats[static_cast<std::size_t>(i)];
    params.actorMaterials[kFirstMaterial + i].crease = i == kWrap || i == kGlove || i == kBrass || i == kWorn || i == kGrime;
  }
}

}  // namespace desert
