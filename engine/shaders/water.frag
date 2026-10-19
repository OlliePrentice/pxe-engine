#version 410 core

// #include "common.glsl"

// Water, after the pond in t3ssel8r's scene video. The scene behind the
// sheet (the pond bed) and its depth were copied before the water drew, so
// each pixel knows how deep the water is in front of what it covers.
// Shallow water shows the bed through a blue tint; deeper water shows the
// mirrored scene, or the sky's stepped blotches where nothing is mirrored;
// and where the water is almost touching something - the shore, a stone,
// the trunk - a thin light contact line is drawn.

in vec3 vWorld;

uniform sampler2D uBed;         // scene colour under the water
uniform sampler2D uBedDepth;    // and its depth
uniform sampler2D uReflection;  // mirrored scene; alpha 0 where only sky
uniform float uDepthMetres;     // far - near: the ortho depth buffer is linear
uniform float uTime;
uniform float uWindTime;    // the wind's phase
uniform vec2 uWindDir;      // world XZ, unit
uniform float uWindRipple;  // 0 calm .. 1 a strong wind ruffling the surface

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;

const float kContact = 0.1;   // metres of water along the view ray that count as touching
const float kClear = 1.4;      // metres along the ray over which the bed fades from view

float rippleHash(vec2 c, float k) {
    return fract(sin(dot(c, vec2(127.1, 311.7)) + k * 74.7) * 43758.5453);
}

// Rain on the water: each 0.9 m cell may hold a drop whose ring grows and
// fades, drawn one pixel thick (the view squashes it into an ellipse).
// Neighbouring cells are checked so rings cross cell borders.
float rainRipple(vec2 xz) {
    if (uRain <= 0.0) return 0.0;
    const float kCell = 0.9;
    float pixel = fwidth(xz.x) * 0.9;  // about one pixel, in metres
    vec2 id = floor(xz / kCell);
    float best = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            vec2 c = id + vec2(x, y);
            if (rippleHash(c, 0.0) > uRain * 0.85) continue;
            float phase = fract(uTime * 0.9 + rippleHash(c, 1.0));
            vec2 centre = (c + vec2(rippleHash(c, 2.0), rippleHash(c, 3.0))) * kCell;
            float r = 0.05 + phase * 0.5;
            if (abs(length(xz - centre) - r) < pixel) best = max(best, 0.75 * (1.0 - phase));
        }
    return best >= 0.25 ? best : 0.0;
}

void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec3 n = vec3(0.0, 1.0, 0.0);
    oNormal = vec4(n * 0.5 + 0.5, float(kSurfWater + 1) / 8.0);

    float depth = (texelFetch(uBedDepth, p, 0).r - gl_FragCoord.z) * uDepthMetres;
    float lit = sunLit(vWorld, n, 1.0) * horizonLight(vWorld);
    if (uFlash > 0.0) lit = mix(lit, uFlashDir.y > 0.0 ? flashLit(vWorld, n, 1.0) : 0.0, uFlash);
    float tone = 2.0 * lit;
    float shade = cloudShade(vWorld);

    if (depth < kContact) {
        oColor = vec4(fireLit(clouded(toneColour(kSurfFoam, tone), kSurfFoam, shade), kSurfFoam, vWorld, n, lit * (1.0 - shade)), 1.0);
        return;
    }

    vec3 body = toneColour(kSurfWater, tone);
    vec3 glint = toneDetail(kSurfWater, tone);

    // The reflection wobbles a pixel sideways in slow bands, like a light
    // ripple, and stays on whole pixels.
    float rip = sin(vWorld.z * 2.7 + vWorld.x * 0.6 + uTime * 1.3) + 0.5 * sin(vWorld.x * 4.1 - uTime * 1.9);
    ivec2 rp = p + ivec2(rip > 1.0 ? 1 : (rip < -1.0 ? -1 : 0), 0);
    // A strong wind breaks the reflection up further, by up to two pixels.
    if (uWindRipple > 0.0) {
        float w = rip + uWindRipple * 1.2 * sin(dot(vWorld.xz, uWindDir) * 5.3 - uWindTime * 4.1);
        rp = p + ivec2(int(clamp(floor(w * (0.7 + 0.5 * uWindRipple) + 0.5), -2.0, 2.0)), 0);
    }
    vec4 refl = texelFetch(uReflection, rp, 0);

    // Sky: drifting blotches in three steps between the body colour and the
    // glint colour.
    vec2 sq = vWorld.xz * 0.16 + uCloudOffset * 2.0;
    float c = 0.6 * cloudNoise(sq) + 0.4 * cloudNoise(sq * 2.3 + 5.0);
    vec3 sky = c > 0.62 ? glint : (c > 0.5 ? mix(body, glint, 0.5) : body);
    vec3 mirror = refl.a > 0.5 ? mix(refl.rgb, body, 0.45) : sky;

    // The bed reads dark and cool through the water, as in the video.
    vec3 bed = mix(texelFetch(uBed, p, 0).rgb * 0.72, body * 0.82, 0.45);
    float ring = rainRipple(vWorld.xz);
    // One irregular edge between the dark shallows and the mirrored deep
    // water, broken up by noise so it reads painted rather than as contour
    // rings.
    float edge = depth / kClear + (cloudNoise(vWorld.xz * 1.4) - 0.5) * 0.7;
    vec3 col = edge > 0.45 ? mirror : bed;
    col = mix(col, toneColour(kSurfFoam, tone), ring);
    // Wind ripples: in a moderate breeze up, patches of fine wavelets
    // (catspaws) drift downwind across the surface, each wavelet a short
    // one-pixel light dash across the wind. The patches grow as it blows
    // harder.
    if (uWindRipple > 0.0) {
        vec2 side = vec2(-uWindDir.y, uWindDir.x);
        float along = dot(vWorld.xz, uWindDir), across = dot(vWorld.xz, side);
        float gustPatch = cloudNoise(vec2(along * 0.35 - uWindTime * 0.6, across * 0.5));
        float crest = fract(along * 2.6 - uWindTime * 1.4 + 0.35 * sin(across * 1.9));
        float dash = fract(across * 1.1 + 0.6 * sin(along * 0.9) + floor(along * 2.6 - uWindTime * 1.4) * 0.37);
        if (gustPatch > 0.85 - 0.35 * uWindRipple && dash < 0.45 && crest < fwidth(along * 2.6) * 1.1)
            col = mix(col, glint, 0.4);
    }
    oColor = vec4(fireLit(clouded(col, kSurfWater, shade), kSurfWater, vWorld, n, lit * (1.0 - shade)), 1.0);
}
