#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aSurface;

uniform mat4 uViewProj;

out vec3 vWorld;
out vec3 vNormal;
flat out int vSurface;

void main() {
    vWorld = aPos;
    vNormal = aNormal;
    vSurface = int(aSurface + 0.5);
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
