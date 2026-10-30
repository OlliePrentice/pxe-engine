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

// Ripples (Scene::ripples), for the one surface they are set on: the normal
// tilted along the wind in an asymmetric wave, shading only.
uniform int uRippleSurface;  // -1 none
uniform float uRippleWavelength, uRippleDepth, uRippleFadeStart, uRippleFadeEnd;
uniform float uRippleMaxSlope;  // radians
uniform vec2 uRippleWind;       // unit heading in xz: (sin h, cos h)
uniform vec3 uEye;

vec3 rippled(vec3 n, vec3 wpos) {
    float dist = length(wpos - uEye);
    float slope = acos(clamp(n.y, -1.0, 1.0));
    float fade = (1.0 - smoothstep(uRippleFadeStart, uRippleFadeEnd, dist))
               * (1.0 - smoothstep(uRippleMaxSlope - radians(4.0), uRippleMaxSlope, slope));
    if (fade <= 0.0) return n;
    vec2 wind = uRippleWind;
    float along = dot(wpos.xz, wind);                      // across the crests, with the wind
    float side = dot(wpos.xz, vec2(wind.y, -wind.x));       // along the crests
    float wander = 0.6 * cloudNoise(vec2(side, along) / (uRippleWavelength * 6.0));
    float u = fract(along / uRippleWavelength + wander);
    // Gentle rise over 0..0.75 (tilting toward the upwind side by a third of
    // the depth), steep fall over 0.75..1 (tilting downwind by the full depth),
    // with short smooth turns between them.
    float steep = smoothstep(0.70, 0.78, u) * (1.0 - smoothstep(0.96, 1.0, u));
    float tilt = uRippleDepth * mix(1.0 / 3.0, -1.0, steep);
    return normalize(n - vec3(wind.x, 0.0, wind.y) * tilt * fade);
}

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
            oColor = vec4(toSrgb(litColour(uActorAlbedo[m], vWorld, n, 1.5, 0.0)), 1.0);
        return;
    }
    oNormal = vec4(n * 0.5 + 0.5, float(vSurface + 1) / kSurfaceScale);
    if (uShowNdl == 1) {
        oColor = vec4(vec3(clamp(dot(n, uSunDir) * 0.5 + 0.5, 0.0, 1.0)), 1.0);
        return;
    }
    // Ripples change the colour's normal only: oNormal above stays the
    // geometric one, so the edge pass sees no ripples.
    if (vSurface == uRippleSurface) n = rippled(n, vWorld);
    // Built stone takes the bigger shadow bias: it sits up to ~12 cm off its
    // flat box face, and needs a bias bigger than its own bumps.
    oColor = vec4(toSrgb(litColour(uAlbedo[vSurface], vWorld, n, flatFaced(vSurface) ? 2.5 : 1.0, uTerminator[vSurface])), 1.0);
}
