#include "tavern.hpp"

#include "scene.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace game {
namespace {

constexpr float kPi = 3.14159265358979323846f;
// The door's leaf is 0.12 m thick; the closed leaf keeps the hero this far
// either side of its middle.
constexpr float kLeafHalf = 0.06f;

glm::vec2 xz(glm::vec3 p) { return {p.x, p.z}; }

}  // namespace

bool Doorway::inZone(glm::vec2 p) const {
  // Into the frame: turned back by its yaw about its origin.
  const glm::vec2 d = p - frameOrigin;
  const float c = std::cos(frameYaw), s = std::sin(frameYaw);
  const glm::vec2 local{c * d.x - s * d.y, s * d.x + c * d.y};
  return local.x >= zoneLo.x && local.x <= zoneHi.x && local.y >= zoneLo.y && local.y <= zoneHi.y;
}

// The exterior (tavern_exterior.glb, assets.md), in its own coordinates and
// placed by tavernPoint: the door hangs at (-1.1, 0, -0.25) at the exterior's
// yaw, its leaf closed along z = -0.19 from x -1.1 to 1.1; the doorway's
// middle is the origin; the vestibule behind it runs back to z = -3. The
// zone starts 0.45 m in from the wall's inside face, past where the open
// door's leaf lies (x -1.49 to -1.1). The hero arrives 2.3 m out on the
// lawn, clear of the threshold step and the eaves, within reach of the door.
Doorway ruinsDoorway() {
  Doorway d;
  d.hinge = tavernPoint({-1.1f, 0.f, -0.25f});
  d.hingeYaw = kTavernYaw;
  d.centre = xz(tavernPoint({0.f, 0.f, 0.f}));
  d.leafA = xz(tavernPoint({-1.1f, 0.f, -0.19f}));
  d.leafB = xz(tavernPoint({1.1f, 0.f, -0.19f}));
  d.frameOrigin = xz(kTavernSpot);
  d.frameYaw = kTavernYaw;
  d.zoneLo = {-1.1f, -2.2f};
  d.zoneHi = {1.1f, -0.7f};
  d.arrive = xz(tavernPoint({0.f, 0.f, 2.3f}));
  d.arriveYaw = kTavernYaw;
  return d;
}

// The room (tavern_interior.glb, assets.md), whose coordinates are the
// scene's: the door hangs at (11.3, 0, 0) turned a quarter-turn (its +z, the
// way it faces closed, along the room's +x), its leaf closed along x = 11.36
// from z 0 to -2.2; the doorway's middle is (11.5, -1.1), mid-wall; the step
// outside runs from the wall's outer face (x 11.7) to x = 14.1. The zone
// starts 0.35 m out from that face, across the clear opening. The hero
// arrives at (8.7, -1.1), 2.8 m from the doorway's middle (within reach, so
// the door opens as it arrives and closes as it walks in) and 2.82 m from
// the hinge, clear of the swing, facing -x.
Doorway tavernDoorway() {
  Doorway d;
  d.hinge = {11.3f, 0.f, 0.f};
  d.hingeYaw = 0.5f * kPi;
  d.centre = {11.5f, -1.1f};
  d.leafA = {11.36f, 0.f};
  d.leafB = {11.36f, -2.2f};
  d.zoneLo = {12.05f, -2.2f};
  d.zoneHi = {14.1f, 0.f};
  d.arrive = {8.7f, -1.1f};
  d.arriveYaw = -0.5f * kPi;
  return d;
}

// Each chair and stool by its middle and the radius that bounds it, and the
// brazier by its footprint's circle (assets.md). Each bench, a seat 3.6 m
// long and 0.45 m wide along x, as a capsule down its middle from end to
// end, half its width round, which covers all of it. The near walls (-x
// and -z, shadow-only, drawn as stubs) along their middles from outer
// corner to outer corner, half their 0.4 m thickness round.
std::vector<Blocker> tavernBlockers() {
  constexpr float kChair = 0.467f, kStool = 0.28f, kBrazier = 0.456f;
  constexpr float kBenchHalfLength = 1.8f, kBenchHalfWidth = 0.225f;
  const glm::vec2 chairs[] = {{-6.05f, 2.05f},  {-8.55f, 1.95f},  {-7.24f, 0.65f},  {-7.12f, 3.15f},  {-2.38f, 0.65f},
                              {-2.26f, 3.15f},  {3.8f, 2.05f},    {1.3f, 1.95f},    {2.61f, 0.65f},   {2.73f, 3.15f},
                              {-6.05f, -3.95f}, {-8.55f, -4.05f}, {-7.24f, -5.35f}, {-7.12f, -2.85f}, {5.47f, -3.95f},
                              {2.93f, -4.05f},  {4.26f, -5.37f}};
  const glm::vec2 stools[] = {{7.05f, 7.8f}, {7.05f, 6.9f}, {7.05f, 6.f}, {7.05f, 5.1f}, {7.05f, 4.2f}, {7.05f, 3.3f}};
  const glm::vec2 benches[] = {{-1.6f, -5.125f}, {-1.6f, -3.075f}};
  const glm::vec2 brazier{8.6f, -4.4f};
  std::vector<Blocker> out;
  for (const glm::vec2& c : chairs) out.push_back({c, c, kChair});
  for (const glm::vec2& c : stools) out.push_back({c, c, kStool});
  for (const glm::vec2& c : benches)
    out.push_back({c - glm::vec2{kBenchHalfLength, 0.f}, c + glm::vec2{kBenchHalfLength, 0.f}, kBenchHalfWidth});
  out.push_back({brazier, brazier, kBrazier});
  out.push_back({{-11.5f, -8.9f}, {-11.5f, 8.9f}, 0.2f});
  out.push_back({{-11.7f, -8.7f}, {11.7f, -8.7f}, 0.2f});
  return out;
}

pxe::SkinnedModel TavernDoor::loadModel() {
  pxe::SkinnedModel model = pxe::SkinnedModel::load(std::string(PXE_ASSET_DIR) + "/scenes/tavern_door.glb", kFirstMaterial);
  if (model.joint("hinge") < 0) throw std::runtime_error("tavern_door.glb has no hinge joint");
  return model;
}

void TavernDoor::build(const pxe::SkinnedModel& model, const Doorway& doorway) {
  model_ = &model;
  doorway_ = doorway;
  hinge_ = model.joint("hinge");
  open_ = 0.f;
  held_ = false;
  model.restPose(pose_);
  update(0.f, std::nullopt);
}

void TavernDoor::holdOpen() {
  held_ = true;
  open_ = 1.f;
  update(0.f, std::nullopt);
}

void TavernDoor::update(float dt, std::optional<glm::vec2> hero) {
  if (!model_) return;
  if (!held_) {
    const bool near = hero && glm::distance(*hero, doorway_.centre) <= kReach;
    open_ = std::clamp(open_ + (near ? dt : -dt) / kSwing, 0.f, 1.f);
  }
  // Eased in and out along the swing: a door turning back mid-swing starts
  // back from the angle it has reached.
  const float eased = open_ * open_ * (3.f - 2.f * open_);
  model_->restPose(pose_);
  model_->turn(pose_, hinge_, {0.f, 1.f, 0.f}, glm::radians(kOpenDegrees) * eased);
  const glm::mat4 place = glm::rotate(glm::translate(glm::mat4{1.f}, doorway_.hinge), doorway_.hingeYaw, {0.f, 1.f, 0.f});
  model_->skin(pose_, place, joints_);
}

void TavernDoor::block(std::vector<Blocker>& out) const {
  if (!model_ || fullyOpen()) return;
  out.push_back({doorway_.leafA, doorway_.leafB, kLeafHalf});
}

void TavernDoor::pose(std::vector<pxe::ActorDraw>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]) {
  if (!model_) return;
  actors.push_back({&model_->mesh(), joints_});
  const auto& mats = model_->materials();
  const int first = model_->firstMaterial();
  for (int i = 0; i < static_cast<int>(mats.size()) && first + i < pxe::kActorMaterials; ++i)
    materials[first + i] = mats[static_cast<std::size_t>(i)];
}

}  // namespace game
