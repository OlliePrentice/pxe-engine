#version 410 core

// #include "common.glsl"

in vec3 vWorld;
in vec3 vNormal;
flat in int vSurface;

uniform float uTime;
uniform int uShowNdl;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;

// Characters: surface kActorSurface + an actor-material slot, whose base
// colour comes from uActorColour. They take the scene's light through its
// stone ramp - uActorTint is the stone's shadow, mid and lit colours now over
// its mid colour by day - so they darken and cool with the evening as the
// scene's stone does.
// Their bands sit lower than the ground's, since most of a figure faces
// sideways. Glowing materials (eyes, a blade) ignore the light.
const int kActorSurface = 16;
const int kActorMaterials = 64;  // pxe::kActorMaterials
uniform vec3 uActorColour[kActorMaterials];
// Per slot: 1 glows (pxe::ActorMaterial::glow), 2 creases (::crease).
uniform int uActorFlags[kActorMaterials];
const int kActorGlows = 1;
const int kActorCreases = 2;
// The normal target's surface for an actor's creased material, past the
// scene's surfaces: edge.frag treats it as stone, and inks its creases too.
const int kSurfActorCrease = 11;
uniform vec3 uActorTint[3];
uniform vec3 uActorFire;  // a base colour's tint in full firelight

float actorTone(vec3 wpos, vec3 n) {
    vec3 L = uSunDir;
    float t = 0.0;
    if (L.y > 0.0) {
        float ndl = dot(n, L);
        t = clamp((ndl - 0.02) / kToneSoftness + 0.5, 0.0, 1.0) + clamp((ndl - 0.42) / kToneSoftness + 0.5, 0.0, 1.0);
        if (t > 0.0) t *= sunLit(wpos, n, 1.5) * horizonLight(wpos);
    }
    if (uFlash > 0.0) {
        float f = clamp(dot(n, uFlashDir) * 2.0 + 0.5, 0.0, 2.0);
        if (f > 0.0) f *= flashLit(wpos, n, 1.5);
        t = mix(t, f, uFlash);
    }
    return t;
}

vec3 actorColour(int m, vec3 n) {
    vec3 a = uActorColour[m];
    if ((uActorFlags[m] & kActorGlows) != 0) return a;
    float tone = actorTone(vWorld, n);
    vec3 col = rampColour(a * uActorTint[0], a * uActorTint[1], a * uActorTint[2], tone);
    float shade = cloudShade(vWorld);
    col = mix(col, a * uActorTint[0], shade);
    float sunlight = 0.5 * tone * (1.0 - shade);
    for (int i = 0; i < uFireCount; ++i) {
        vec3 d = uFirePos[i] - vWorld;
        float dist = length(d);
        if (dist > uFireReach) continue;
        float ndl = clamp(dot(n, d / max(dist, 1e-3)) * 0.8 + 0.2, 0.0, 1.0);
        float k = pow(1.0 - dist / uFireReach, 1.4) * ndl * uFireLight[i] * (1.0 - uFireDim * clamp(sunlight, 0.0, 1.0));
        if (k <= 0.003) continue;
        if (uFireCube[i] >= 0) k *= mix(1.0, fireVisible(vWorld, n, uFirePos[i], uFireCube[i]), uFireShadowed[i]);
        col = mix(col, max(col, a * uActorFire), clamp(k, 0.0, 0.85));
    }
    return col;
}

void main() {
    vec3 n = normalize(vNormal);
    if (vSurface >= kActorSurface) {
        // Outlined and rimmed in the edge pass as stone is; a creased
        // material is inked along its creases as well.
        int m = clamp(vSurface - kActorSurface, 0, kActorMaterials - 1);
        int surface = (uActorFlags[m] & kActorCreases) != 0 ? kSurfActorCrease : kSurfStone;
        oNormal = vec4(n * 0.5 + 0.5, float(surface + 1) / kSurfaceScale);
        oColor = vec4(actorColour(m, n), 1.0);
        return;
    }
    oNormal = vec4(n * 0.5 + 0.5, float(vSurface + 1) / kSurfaceScale);
    if (uShowNdl == 1) {
        oColor = vec4(vec3(clamp(dot(n, uSunDir) * 0.5 + 0.5, 0.0, 1.0)), 1.0);
        return;
    }
    // A crown's body sits a band darker than its leaves, so gaps between
    // leaves read as depth inside the foliage.
    float tone = flatFaced(vSurface)       ? toonToneFlat(vWorld, n)
               : (vSurface == kSurfBark)  ? barkTone(vWorld, n)
               : (vSurface == kSurfLeaf)  ? max(floor(toonTone(vWorld, n) + 0.5) - 1.0, 0.0)
                                          : toonTone(vWorld, n);
    // The direct light alone, which drowns out firelight by day (fireLit).
    float sunTone = tone;
    vec3 col = uIndoors != 0 ? indoorColour(vSurface, tone, vWorld, n, flatFaced(vSurface)) : toneColour(vSurface, tone);
#ifdef FIRE_GRID
    // The lawn under burnt grass is scorched, then charred. A burning crown
    // chars and burns away in holes, and the trunk under it blackens.
    float charred = 0.0;
    if (vSurface == 0) {
        float burn = burnAt(vWorld);
        if (burn > 0.3) col = burntColour(tone, burn > 0.62 ? 2 : 1);
        charred = smoothstep(0.3, 0.75, burn);
    } else if (vSurface == kSurfLeaf || vSurface == kSurfBark) {
        float burnt = crownBurnAt(vWorld);
        if (vSurface == kSurfLeaf && crownGone(burnt, vWorld)) discard;
        charred = smoothstep(0.15, 0.6, burnt);
        if (charred > 0.0) col = mix(col, burntColour(tone, 2) * (vSurface == kSurfBark ? 0.8 : 1.0), charred);
    }
    float shade = cloudShade(vWorld);
    col = clouded(col, vSurface, shade);
    oColor = vec4(charLit(col, fireLit(col, vSurface, vWorld, n, 0.5 * sunTone * (1.0 - shade)), charred), 1.0);
#else
    float shade = cloudShade(vWorld);
    col = clouded(col, vSurface, shade);
    oColor = vec4(fireLit(col, vSurface, vWorld, n, 0.5 * sunTone * (1.0 - shade)), 1.0);
#endif
}
