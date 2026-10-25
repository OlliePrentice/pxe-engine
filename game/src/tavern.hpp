#pragma once

#include "hero.hpp"

#include "pxe/renderer.hpp"
#include "pxe/skinned.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <iterator>
#include <optional>
#include <vector>

namespace game {

// The Sleeping Fox: its exterior stands on the ruins' lawn (scene.hpp,
// kTavernSpot) and its one room is a scene of its own, "tavern"
// (buildScene). A door hangs in each of its two doorways - the exterior's
// and the room's - and opens on its own for the hero; walking through the
// open doorway takes the hero to the other scene, behind a loading screen
// that shows one of these, picked at random each time.
inline constexpr const char* kTavernLore[] = {
    "Long before the pillars fell, travellers sheltered here on the road east. The temple is gone, but the Sleeping "
    "Fox keeps its old hearth burning.",
    "The foxes of the ruins curl up by any fire left untended. The innkeeper swears they always know when the stew "
    "is on.",
    "No one remembers who laid the stones of the plaza. The old songs say they were raised in a single night, and "
    "broken in another.",
    "Rain cannot reach a fire beneath a roof. Hooded wanderers learned that long before they learned to call fire "
    "from their palms.",
    "When lightning walks the hills, the regulars count the seconds to the thunder, and drink one for each.",
    "The pond below the ruins is said to show the sky as it was, not as it is. Fishermen come back with no fish and "
    "strange stories.",
};
inline constexpr int kTavernLoreCount = static_cast<int>(std::size(kTavernLore));

// The room as the camera frames it (tavern_interior.glb, assets.md): its
// inner bounds, x -11.3 to 11.3 and z -8.5 to 8.5, out to the outer faces
// of its 0.4 m walls, from the floor up to its roof's underside. At the
// design yaw it projects to 29.1 x 18.6 m, inside the 40 x 22.5 m view, so
// indoors the camera's target is kept where all of it stays in view.
constexpr glm::vec3 kRoomMin{-11.7f, 0.f, -8.9f};
constexpr glm::vec3 kRoomMax{11.7f, 4.55f, 8.9f};

// One of the tavern's doorways, in the scene it is in. Ground positions are
// world xz; yaws are radians about +y, 0 facing +z.
struct Doorway {
  // Where its door hangs: the door model's origin (the foot of its hinge
  // edge) and yaw.
  glm::vec3 hinge{0.f};
  float hingeYaw = 0.f;
  // The doorway's middle: the door opens while the hero is within
  // TavernDoor::kReach of it.
  glm::vec2 centre{0.f};
  // The closed door's leaf, along its middle: the hero is kept off it
  // until the door is fully open.
  glm::vec2 leafA{0.f}, leafB{0.f};
  // Walking into this zone through the open doorway takes the hero to the
  // other scene: a rectangle, lo to hi, in the frame at `frameOrigin` turned
  // by `frameYaw` (the asset's own coordinates).
  glm::vec2 frameOrigin{0.f};
  float frameYaw = 0.f;
  glm::vec2 zoneLo{0.f}, zoneHi{0.f};
  // Where the hero appears, and the way it faces, coming in through this
  // doorway from the other scene.
  glm::vec2 arrive{0.f};
  float arriveYaw = 0.f;

  bool inZone(glm::vec2 xz) const;
};
// The exterior's doorway, in the ruins: its zone is just inside the
// threshold, in the vestibule, and the hero arrives on the lawn outside,
// facing away from the tavern.
Doorway ruinsDoorway();
// The room's doorway, in the tavern: its zone is on the step outside the
// inside door, and the hero arrives just inside it, facing into the room.
Doorway tavernDoorway();

// Where the hero may not go in the room besides its ground: every chair,
// stool and bench and the brazier, which are left out of the ground, and
// the two near walls, which the camera sees only as stubs a step high.
std::vector<Blocker> tavernBlockers();

// A tavern door (tools/blender/build_tavern_door.py), hung in a doorway. One
// model serves every door. It swings into the building by kOpenDegrees over
// kSwing seconds while the hero is within kReach of the doorway's middle,
// and closes over as long once the hero is farther; the swing eases in and
// out, and turning back mid-swing carries on from where it is. Foxes do not
// open it and its swing pushes nothing; until it is fully open its leaf
// keeps the hero out of the doorway.
class TavernDoor {
 public:
  static constexpr float kOpenDegrees = 100.f;
  static constexpr float kSwing = 0.4f;
  static constexpr float kReach = 3.f;
  // Its two colours (Door_Wood, Door_Iron) take these actor-material slots,
  // after the hero's 0-7 and the fox's 8-11.
  static constexpr int kFirstMaterial = 12;

  // The model every door is drawn from, loaded once (needs the GL context);
  // throws if it is missing or has no `hinge` joint.
  static pxe::SkinnedModel loadModel();
  // Hang in `doorway`, closed, drawn from `model` (which must outlive it).
  void build(const pxe::SkinnedModel& model, const Doorway& doorway);
  bool built() const { return model_ != nullptr; }
  // Shown fully open from now on, whoever is near (--tavern-door-open).
  void holdOpen();
  // Swing on by dt toward open while the hero (if there is one: xz) is near
  // the doorway, toward closed otherwise.
  void update(float dt, std::optional<glm::vec2> hero);
  bool fullyOpen() const { return open_ >= 1.f; }
  // The closed leaf, while the door is not fully open, into `out`.
  void block(std::vector<Blocker>& out) const;
  // Hand this moment's door to the frame (its joint matrices last until the
  // next update), its colours into its slots.
  void pose(std::vector<pxe::ActorDraw>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]);

 private:
  const pxe::SkinnedModel* model_ = nullptr;
  Doorway doorway_;
  int hinge_ = -1;
  float open_ = 0.f;  // 0 closed .. 1 open, along the swing before easing
  bool held_ = false;
  pxe::SkinnedModel::Pose pose_;
  std::vector<glm::mat4> joints_;
};

}  // namespace game
