#pragma once

#include "pxe/heightmap.hpp"
#include "pxe/mesh.hpp"
#include "pxe/shelter.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace pxe {

// One grass or leaf sprite, drawn procedurally in the fragment shader. The
// whole quad is lit from `root` and `normal`.
struct GrassInstance {
  float px, py, pz;
  float nx, ny, nz;
  float kind;     // 0 blade clump, 1 tuft, 2 leaf cluster, 3 leaf cluster in the detail colour
  float seed;     // [0, 1), picks blade heights and leans
  float scale;    // blade height multiplier
  float surface;  // Surface whose ramp colours the sprite
};

static_assert(sizeof(GrassInstance) == sizeof(float) * 10, "GrassInstance is tightly packed");

// Colours for one surface, indexed by toon band: 0 shadow, 1 mid, 2 lit.
struct Palette {
  glm::vec3 band[3]{};
  glm::vec3 detail[3]{};  // tufts, leaf speckles, water streaks
  glm::vec3 tip[3]{};     // a blade's top pixel over darker ground
};

// Palette variants the day/night cycle blends between by sun elevation,
// after t3ssel8r's procedural day/night cycle.
enum Phase : int {
  kPhaseDay = 0,
  kPhaseGolden = 1,
  kPhaseTwilight = 2,
  kPhaseNight = 3,
  kPhaseCount = 4,
};

// A fire the scene starts with: where its flames rise from, whether a
// structure roofs it over (from `Scene::shelter`), so rain cannot reach it,
// and how big it is (1 a campfire, about 0.2 a candle; see FireDraw).
struct FireSite {
  glm::vec3 base{0.f};
  bool sheltered = false;
  float size = 1.f;
};

struct Scene {
  std::vector<Mesh> meshes;  // opaque; all cast shadows
  // Drawn only into the sun and moon's shadow map (and lightning's), never
  // seen: a cutaway room's roof and the walls nearest the camera, so the
  // room is lit as if it were closed while the camera sees in. Built from
  // glTF nodes named "..._ShadowOnly". They are not part of `ground`, but
  // are of `shelter`, and of the bounds the shadow map is fitted to.
  std::vector<Mesh> shadowOnly;
  // Water sheets, drawn after the opaque meshes with reflections. They do
  // not cast shadows. waterLevel is the height of the (flat) water plane,
  // and waterMin/Max bound the sheets for the reflection pass's scissor.
  std::vector<Mesh> water;
  float waterLevel = 0.f;
  glm::vec3 waterMin{0.f};
  glm::vec3 waterMax{0.f};
  std::vector<GrassInstance> grass;
  // Fires the scene starts with. The game owns them from then on, and can
  // add, move and remove fires as it runs (see FrameParams::fires).
  std::vector<FireSite> fires;
  // Built structure overhead, for keeping rain off fires.
  ShelterMap shelter;
  // The top of the ground and everything standing on it (not tree crowns),
  // and of the water, for things that roll over the scene.
  HeightMap ground;
  HeightMap waterTop;
  // Where each firefly wanders round, when they are out.
  std::vector<glm::vec3> fireflyHomes;
  Palette palette[kPhaseCount][kSurfCount];
  // Edge-pass colours per phase: light rim on stone tops, dark outline.
  glm::vec3 rim[kPhaseCount]{};
  glm::vec3 ink[kPhaseCount]{};

  // N·L thresholds for the toon ramp with the sun high. They scale down as
  // the sun drops so a low sun still lights flat ground.
  float tLit = 0.75f;
  float tMid = 0.585f;

  // Sun path: rises from +east, peaks toward +south at `sunPeakDegrees`.
  glm::vec3 sunEast{1.f, 0.f, 0.f};
  glm::vec3 sunSouth{0.f, 0.f, -1.f};
  float sunPeakDegrees = 55.f;

  // An off-screen ridge of hills and trees ringing the scene. The sun has to
  // clear it before it reaches a point, so at sunrise the light finds the
  // tops of tall things first and its edge sinks down them as the sun
  // climbs. Radius 0 turns it off.
  glm::vec2 horizonCentre{0.f};
  float horizonRadius = 0.f;
  float horizonHeight = 0.f;  // mean ridge height above y = 0, metres
  float horizonRelief = 0.f;  // how far the ridge rises and dips around that

  glm::vec3 boundsMin{0.f};
  glm::vec3 boundsMax{0.f};

  // A room under a roof, with no sky in view: no rain is drawn over the
  // frame (the edge pass's streaks, splashes and murk), no cloud shadows
  // drift over it and no god rays shine in. The rain's overcast still dims
  // the colours, and sun, moon and lightning still light it through its
  // windows by the shadow map.
  bool indoors = false;
  // Indoors only: toward the room's fill light, a fixed light from above
  // that lifts each face out of shadow by how far it turns toward it, so
  // the sides of furniture read apart (Lighting::fill says how strong).
  glm::vec3 fillDir{0.f, 1.f, 0.f};

  // Where the camera starts.
  glm::vec3 focus{0.f};
  float yawDegrees = 45.f;
};

}  // namespace pxe
