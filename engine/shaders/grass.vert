#version 410 core

// #include "common.glsl"

uniform mat4 uViewProj;
uniform vec2 uTarget;
uniform vec4 uRect[4];
uniform vec2 uAtlasSize;
uniform float uDepthBiasNdc;
uniform vec3 uAccent[5];

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec3 iRoot;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in vec4 iParams;  // sprite, flip, accent, unused

out vec2 vUv;
flat out vec3 vCol;

void main() {
    vec4 c = uViewProj * vec4(iRoot, 1.0);
    vec2 ndc = c.xy / c.w;
    vec2 px = floor((ndc * 0.5 + 0.5) * uTarget);
    vec4 r = uRect[int(iParams.x + 0.5)];
    vec2 p = px + vec2(aCorner.x * r.z - floor(r.z * 0.5), aCorner.y * r.w);
    gl_Position = vec4(p / uTarget * 2.0 - 1.0, c.z / c.w - uDepthBiasNdc, 1.0);

    float u = (iParams.y > 0.5) ? (1.0 - aCorner.x) : aCorner.x;
    vUv = (r.xy + vec2(u, aCorner.y) * r.zw) / uAtlasSize;

    vec3 n = normalize(iNormal);
    vec3 col = toonShade(iRoot, n);
    if (iParams.z >= 0.0) {
        float bandL = dot(col, vec3(0.299, 0.587, 0.114));
        int start = int(floor(iParams.z));
        if (start < 0) start = 0;
        if (start > 4) start = 4;
        for (int k = 0; k < 5; ++k) {
            int i = start + k;
            if (i >= 5) i -= 5;
            float accentL = dot(uAccent[i], vec3(0.299, 0.587, 0.114));
            if (abs(accentL - bandL) >= (25.0 / 255.0)) {
                col = uAccent[i];
                break;
            }
        }
    }
    vCol = col;
}
