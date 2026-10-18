#version 410 core

// #include "common.glsl"

in vec3 vWorld;

uniform sampler2D uNormalTex;
uniform float uWorldMin;
uniform float uWorldSize;
uniform int uShowNdl;

out vec4 oColor;

void main() {
    vec2 uv = (vWorld.xz - vec2(uWorldMin)) / uWorldSize;
    vec3 n = normalize(texture(uNormalTex, uv).xyz);
    if (uShowNdl == 1) {
        float ndl = dot(n, uSunDir) * 0.5 + 0.5;
        oColor = vec4(vec3(clamp(ndl, 0.0, 1.0)), 1.0);
        return;
    }
    oColor = vec4(toonShade(vWorld, n), 1.0);
}
