#version 410 core

in vec2 vUv;

uniform sampler2D uTex;

out vec4 oColor;

void main() {
    oColor = vec4(texture(uTex, vUv).rgb, 1.0);
}
