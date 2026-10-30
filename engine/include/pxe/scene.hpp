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

// A window in a room's wall, open to the sky (indoors only). The sky lights
// the room through it - a soft light on the floor and walls in front of it,
// besides the sun or moon falling in through the shadow map - and is seen
// through it from inside.
struct SkyWindow {
  glm::vec3 centre{0.f};               // the middle of the opening, in the wall's inner face
  glm::vec3 inward{0.f, 0.f, -1.f};    // level, unit, into the room
  float width = 1.f;                   // the opening, metres
  float height = 1.f;
  float depth = 0.4f;                  // the wall's thickness
};

// A face of a box, in the box's own frame.
enum class BoxFace : int {
  kPosX = 0,
  kNegX = 1,
  kPosY = 2,
  kNegY = 3,
  kPosZ = 4,
  kNegZ = 5,
};

// A dark region: an oriented box that the scene's light does not reach,
// open on one face, such as a deep room or passage seen through an opening.
// Every surface and character inside it, and the edge pass's light rims on
// them, take a fixed share of their surface's mid colour for the time of day
// instead of being lit: no sun, moon, fill, sky light, lightning, cloud
// shade, firelight or point lights. The share is `startStrength` at the open
// face and moves linearly to `endStrength` at `fadeDistance` in from it
// (measured straight in, along the face's normal), holding there beyond it,
// so the region fades from dim at its opening to dark at its back. A
// character's glowing materials (ActorMaterial::glow) still glow, and the
// edge pass's dark outlines are unchanged.
//
// Inside means within the box or on its faces. A surface lying exactly in a
// face's plane may fall either side of it pixel by pixel, so make the box a
// little larger than the floor and walls it darkens, on every face but the
// open one.
struct DarkRegion {
  glm::vec3 centre{0.f};
  glm::vec3 halfExtents{0.5f};
  // Radians about +y. The box's own +x goes to (cos yaw, 0, -sin yaw) and
  // its +z to (sin yaw, 0, cos yaw).
  float yaw = 0.f;
  BoxFace openFace = BoxFace::kPosZ;
  float startStrength = 1.f;  // the share of the mid colour at the open face, 0 black .. 1
  float endStrength = 0.f;    // and from fadeDistance in
  float fadeDistance = 1.f;   // metres; 0 or less is endStrength throughout
};

// One layer of perspective mode's sky clouds (Scene::skyClouds): soft
// streaks on a flat plane `height` metres above the eye, stretched along the
// wind (Lighting::windDir), drifting by Weather::skyCloudOffset times
// `drift`. Coloured from the light; they cast no shadows. Not drawn under Camera.
struct SkyCloudLayer {
  float height = 8000.f;   // metres above the eye
  float cover = 0.f;       // 0 none .. 1 most of the sky
  float opacity = 0.6f;    // 0..1 where a cloud is thickest
  float length = 6000.f;   // metres a streak runs along the wind
  float width = 900.f;     // metres across it
  float drift = 1.f;       // times Weather::skyCloudOffset
};

struct Scene {
  // Starts with the reference palettes, rim and ink (see `palette`), and an
  // empty scene.
  Scene();

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
  // Built structure overhead, which keeps the rain off what stands under it.
  ShelterMap shelter;
  // The top of the ground and everything standing on it (not tree crowns),
  // and of the water, for things that roll over the scene.
  HeightMap ground;
  HeightMap waterTop;
  // Where each firefly wanders round, when they are out.
  std::vector<glm::vec3> fireflyHomes;
  // Each surface's colours per phase. They default to the reference style,
  // sampled from t3ssel8r's videos, for every phase and surface; a game
  // overrides only the ramps it wants different.
  Palette palette[kPhaseCount][kSurfCount];
  // Edge-pass colours per phase: light rim on stone tops, dark outline.
  // Default to the reference style's, like `palette`.
  glm::vec3 rim[kPhaseCount]{};
  glm::vec3 ink[kPhaseCount]{};
  // The sky behind a perspective view (FirstPersonCamera) per phase: a
  // continuous gradient from the horizon colour to the zenith colour.
  // Surfaces fade into the horizon colour toward the far plane. Default to
  // the reference style's, like `palette`. Not drawn under Camera.
  glm::vec3 skyHorizon[kPhaseCount]{};
  glm::vec3 skyZenith[kPhaseCount]{};
  // Perspective mode's light (FirstPersonCamera), a smooth lit model rather
  // than the toon ramp. Per phase, blended by sun elevation as the palettes
  // are: the direct light (the sun's; in kPhaseNight the moon's), the sky's
  // fill from above and the light bounced up from the ground, each a display
  // (sRGB) colour and a linear strength. Not used under Camera.
  glm::vec3 sunColour[kPhaseCount]{};
  float sunStrength[kPhaseCount]{};
  glm::vec3 skyFill[kPhaseCount]{};
  float skyFillStrength[kPhaseCount]{};
  glm::vec3 groundBounce[kPhaseCount]{};
  float groundBounceStrength[kPhaseCount]{};
  // Each surface's base colour (albedo, display sRGB) under perspective
  // mode's light: how it looks facing the noon sun. The constructor sets it
  // to each surface's day mid band (palette[kPhaseDay][s].band[1]); a game
  // that changes that band afterwards sets the albedo itself.
  glm::vec3 albedo[kSurfCount]{};
  // Perspective mode's air and exposure, the same at every hour.
  float hazeDensity = 0.006f;   // per metre: haze is 1 - exp(-density * distance), full at the far plane
  float sunGlow = 0.6f;         // 0..1: how far haze and sky toward the sun take its colour
  float shadowPenumbra = 0.3f;  // metres across a soft shadow's edge
  float exposure = 1.f;         // multiplies the light before it is clamped
  // Perspective mode's sky clouds: [0] a high, thin layer, [1] a lower,
  // fuller one. Both have cover 0 (none) unless the game sets it.
  static constexpr int kSkyCloudLayers = 2;
  SkyCloudLayer skyClouds[kSkyCloudLayers];
  float skyCloudBrightness = 1.f;  // scales the clouds' colour

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
  // Indoors only: the room's windows, and per phase the colour of the
  // sky's light through them (a tint about 1, which the fill light takes
  // too; see Lighting::skyTint) and of the sky seen through them.
  std::vector<SkyWindow> windows;
  glm::vec3 skyTint[kPhaseCount]{};
  glm::vec3 skyColour[kPhaseCount]{};

  // Boxes the scene's light does not reach (see DarkRegion), indoors or
  // out. Read each frame; regions past the renderer's limit of 8 are left
  // out. Empty by default, which leaves every pixel as it was.
  std::vector<DarkRegion> darkRegions;
};

}  // namespace pxe
