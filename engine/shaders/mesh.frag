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
    oColor = vec4(clouded(col, vSurface, cloudShade(vWorld)), 1.0);
}
