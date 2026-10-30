#version 410 core

// The mesh fragment shader of the perspective mode (RenderMode::kPerspective,
// pxe::FirstPersonCamera): the smooth lit model of lit_perspective.glsl in
// place of mesh.frag's toon ramp.
//
// The marker line below is replaced by compose() (shader.cpp) with whatever
// common file linkProgram is given: here lit_perspective.glsl, not
// common.glsl.
// #include "common.glsl"

in vec3 vWorld;
in vec3 vNormal;
flat in int vSurface;

uniform int uShowNdl;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;

// Characters: surface kActorSurface + an actor-material slot. Each material
// has one base colour, its albedo, lit like a scene surface. Glowing
// materials (eyes, a blade) ignore the light and show uActorColour as it is.
const int kActorSurface = 16;
const int kActorMaterials = 64;  // pxe::kActorMaterials
uniform vec3 uActorColour[kActorMaterials];  // display sRGB, for a glowing material
uniform vec3 uActorAlbedo[kActorMaterials];  // linear
// Per slot: 1 glows (pxe::ActorMaterial::glow), 2 creases (::crease).
uniform int uActorFlags[kActorMaterials];
const int kActorGlows = 1;
const int kActorCreases = 2;
// The normal target's surface for an actor's creased material, past the
// scene's surfaces: edge_perspective.frag treats it as stone, and inks its
// creases too.
const int kSurfActorCrease = 12;

void main() {
    vec3 n = normalize(vNormal);
    if (vSurface >= kActorSurface) {
        // Outlined and rimmed in the edge pass as stone is; a creased
        // material is inked along its creases as well.
        int m = clamp(vSurface - kActorSurface, 0, kActorMaterials - 1);
        int surface = (uActorFlags[m] & kActorCreases) != 0 ? kSurfActorCrease : kSurfStone;
        oNormal = vec4(n * 0.5 + 0.5, float(surface + 1) / kSurfaceScale);
        if ((uActorFlags[m] & kActorGlows) != 0)
            oColor = vec4(uActorColour[m], 1.0);
        else
            oColor = vec4(toSrgb(litColour(uActorAlbedo[m], vWorld, n, 1.5)), 1.0);
        return;
    }
    oNormal = vec4(n * 0.5 + 0.5, float(vSurface + 1) / kSurfaceScale);
    if (uShowNdl == 1) {
        oColor = vec4(vec3(clamp(dot(n, uSunDir) * 0.5 + 0.5, 0.0, 1.0)), 1.0);
        return;
    }
    // Built stone takes the bigger shadow bias: it sits up to ~12 cm off its
    // flat box face, and needs a bias bigger than its own bumps.
    oColor = vec4(toSrgb(litColour(uAlbedo[vSurface], vWorld, n, flatFaced(vSurface) ? 2.5 : 1.0)), 1.0);
}
