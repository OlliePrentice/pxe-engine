#version 410 core

// #include "common.glsl"

in vec3 vWorld;
in vec3 vNormal;
flat in int vSurface;

uniform vec3 uDetail[kSurfaces * 3];
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
    if (vSurface == kSurfFoam) {
        oColor = vec4(bandColour(kSurfFoam, 2), 1.0);
        return;
    }
    // Water only takes cast shadow, never N·L: it is a flat mirror.
    int band = (vSurface == kSurfWater) ? (sunVisible(vWorld, n) < 0.5 ? 0 : 2) : toonBand(vWorld, n);
    vec3 col = bandColour(vSurface, band);
    if (vSurface == kSurfWater) {
        // Slow drifting streaks stand in for reflected sky.
        vec2 p = vWorld.xz;
        float s = sin(p.x * 0.9 + p.y * 0.35 + uTime * 0.4) + 0.6 * sin(p.x * 0.31 - p.y * 1.3 - uTime * 0.25);
        if (s > 1.05) col = uDetail[kSurfWater * 3 + band];
    }
    oColor = vec4(col, 1.0);
}
