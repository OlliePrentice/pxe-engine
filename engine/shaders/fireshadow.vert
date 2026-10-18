#version 410 core

// The fire's shadow map: one face of a cube round the fire.

layout(location = 0) in vec3 aPos;

uniform mat4 uLightViewProj;

out vec3 vWorld;

void main() {
    vWorld = aPos;
    gl_Position = uLightViewProj * vec4(aPos, 1.0);
}
