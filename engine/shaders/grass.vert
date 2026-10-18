#version 410 core

// #include "common.glsl"

uniform mat4 uViewProj;
uniform vec2 uTarget;
uniform float uDepthBiasNdc;
uniform vec3 uTip[kSurfaces * 3];     // blade tip colour where it overhangs darker ground
uniform float uTime;
uniform float uWindPx;    // peak tip sway in target pixels
uniform vec2 uWindDir;    // world XZ, unit
uniform float uWindLean;  // how much of the wind points along screen right, -1..1

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec3 iRoot;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in vec4 iParams;  // kind, seed, height scale, surface

// Sprite boxes in target pixels, root at (kBox.x/2, 0). Big enough for the
// widest lean plus sway so nothing clips.
const vec2 kClumpBox = vec2(13.0, 12.0);
const vec2 kTuftBox = vec2(17.0, 10.0);
const vec2 kLeafBox = vec2(9.0, 9.0);  // centred on the root, not grown up from it

out vec2 vLocal;          // pixel offset from the root pixel's corner
flat out vec3 vCol;
flat out vec3 vTip;
flat out float vKind;
flat out float vSeed;
flat out float vScale;
flat out float vSway;

// Two travelling sine gusts across the field. Low frequency so neighbouring
// tufts move together and the motion reads as waves rather than jitter.
// Gusts travel downwind, so they roll across the field the same way the
// cloud shadows drift.
float wind(vec2 xz) {
    vec2 side = vec2(-uWindDir.y, uWindDir.x);
    float a = sin(dot(xz, uWindDir) * 0.25 - uTime * 1.7);
    float b = sin(dot(xz, uWindDir * 0.17 + side * 0.24) - uTime * 2.3 + 1.3);
    return 0.65 * a + 0.35 * b;
}

void main() {
    vec4 c = uViewProj * vec4(iRoot, 1.0);
    vec2 ndc = c.xy / c.w;
    vec2 px = floor((ndc * 0.5 + 0.5) * uTarget);

    vKind = iParams.x;
    bool leaf = vKind > 1.5;
    vec2 box = leaf ? kLeafBox : ((vKind < 0.5) ? kClumpBox : kTuftBox);
    float halfW = floor(box.x * 0.5);
    float halfH = leaf ? floor(box.y * 0.5) : 0.0;
    vLocal = vec2(aCorner.x * box.x - halfW, aCorner.y * box.y - halfH);
    vec2 p = px + vLocal;
    gl_Position = vec4(p / uTarget * 2.0 - 1.0, c.z / c.w - uDepthBiasNdc, 1.0);

    vSeed = iParams.y;
    vScale = iParams.z;
    // Blades lean downwind on screen, with gusts and a little per-tuft phase
    // on top so the edge never moves as one rigid strip.
    vSway = uWindPx * (uWindLean * (0.6 + wind(iRoot.xz)) + 0.25 * sin(uTime * 3.1 + vSeed * 6.2831));

    // Lit once at the root, exactly like the ground under it, so a tuft is
    // invisible inside a band and only shows where it overhangs the next one.
    int surface = int(iParams.w + 0.5);
    float tone = toonTone(iRoot, iNormal);
    bool detail = (vKind > 0.5 && vKind < 1.5) || vKind > 2.5;
    vCol = detail ? toneDetail(surface, tone) : toneColour(surface, tone);
    int i = surface * 3;
    vTip = rampColour(uTip[i], uTip[i + 1], uTip[i + 2], tone);
    float shade = cloudShade(iRoot);
    vCol = clouded(vCol, surface, shade);
    vTip = clouded(vTip, surface, shade);
    if (leaf) vSway *= 0.5;
}
