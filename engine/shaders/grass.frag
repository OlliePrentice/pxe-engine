#version 410 core

in vec2 vUv;
flat in vec3 vCol;

uniform sampler2D uAtlas;
uniform int uMagenta;

out vec4 oColor;

void main() {
    if (texture(uAtlas, vUv).r < 0.5) discard;
    oColor = (uMagenta == 1) ? vec4(1.0, 0.0, 1.0, 1.0) : vec4(vCol, 1.0);
}
