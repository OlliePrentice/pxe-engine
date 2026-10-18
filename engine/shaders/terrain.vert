#version 410 core

layout(location = 0) in vec3 aPos;

uniform mat4 uViewProj;

out vec3 vWorld;

void main() {
    vWorld = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
