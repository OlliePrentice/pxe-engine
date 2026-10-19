#version 410 core

// #include "common.glsl"

in vec3 vWorld;
in vec3 vNormal;
flat in int vSurface;

uniform float uTime;
uniform int uShowNdl;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;

void main() {
    vec3 n = normalize(vNormal);
    oNormal = vec4(n * 0.5 + 0.5, float(vSurface + 1) / 8.0);
    if (uShowNdl == 1) {
        oColor = vec4(vec3(clamp(dot(n, uSunDir) * 0.5 + 0.5, 0.0, 1.0)), 1.0);
        return;
    }
    // A crown's body sits a band darker than its leaves, so gaps between
    // leaves read as depth inside the foliage.
    float tone = (vSurface == kSurfStone) ? toonToneFlat(vWorld, n)
               : (vSurface == kSurfBark)  ? barkTone(vWorld, n)
               : (vSurface == kSurfLeaf)  ? max(floor(toonTone(vWorld, n) + 0.5) - 1.0, 0.0)
                                          : toonTone(vWorld, n);
    vec3 col = toneColour(vSurface, tone);
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
    oColor = vec4(charLit(col, fireLit(col, vSurface, vWorld, n, 0.5 * tone * (1.0 - shade)), charred), 1.0);
#else
    float shade = cloudShade(vWorld);
    col = clouded(col, vSurface, shade);
    oColor = vec4(fireLit(col, vSurface, vWorld, n, 0.5 * tone * (1.0 - shade)), 1.0);
#endif
}
