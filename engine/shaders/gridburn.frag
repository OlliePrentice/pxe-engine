#version 410 core

// Burning cells paint the burn texture, which the grass and the ground
// read: 0 untouched .. 1 burnt out. Drawn with a max blend, so it only
// ever darkens. A cell's burn is the share of its grass gone, blended
// across cells and pushed about by a fixed noise, so the burnt edge is
// ragged and creeps outward unevenly as the grass goes.

uniform sampler2D uState;
uniform sampler2D uGround;
uniform vec2 uMin;      // world xz of the grid's corner
uniform float uTexel;   // metres per burn texel
uniform float uScale;   // burn texels per cell
// Regrowth: redrawn only under these strokes (as in gridstep.frag), with a
// min blend, so the burn lightens where the grass has come back.
uniform int uRegrow;
uniform int uStrokeCount;
uniform vec4 uStroke[8];
uniform vec2 uStrokeShape[8];

out vec4 oBurn;

float hash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

// The state keeps fuel at half precision, so a cell's share burnt starts
// counting a little above nothing: untouched grass must stay untouched.
float burntCell(ivec2 c) {
    c = clamp(c, ivec2(0), textureSize(uState, 0) - 1);
    float fuel = texelFetch(uGround, c, 0).r;
    return fuel > 0.0 ? clamp((1.0 - texelFetch(uState, c, 0).g / fuel - 0.01) / 0.99, 0.0, 1.0) : 0.0;
}

bool regrowing(vec2 cell) {
    for (int i = 0; i < uStrokeCount; ++i) {
        if (uStrokeShape[i].y < 1.5) continue;
        vec2 a = uStroke[i].xy, ab = uStroke[i].zw - uStroke[i].xy;
        float t = clamp(dot(cell - a, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
        if (distance(cell, a + ab * t) < uStrokeShape[i].x) return true;
    }
    return false;
}

void main() {
    if (uRegrow == 1 && !regrowing(gl_FragCoord.xy / uScale)) discard;
    vec2 cell = gl_FragCoord.xy / uScale - 0.5;
    ivec2 i = ivec2(floor(cell));
    vec2 f = cell - vec2(i);
    float b = mix(mix(burntCell(i), burntCell(i + ivec2(1, 0)), f.x),
                  mix(burntCell(i + ivec2(0, 1)), burntCell(i + ivec2(1, 1)), f.x), f.y);
    if (b <= 0.0) {
        if (uRegrow == 0) discard;
        oBurn = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec2 world = uMin + gl_FragCoord.xy * uTexel;
    float n = 0.65 * noise(world * 2.2) + 0.35 * noise(world * 5.3 + 7.7);
    oBurn = vec4(clamp(b * 1.3 + (n - 0.5) * 0.7 * smoothstep(0.0, 0.3, b), 0.0, 1.0), 0.0, 0.0, 1.0);
}
