#version 410 core

layout(location = 0) in vec3 aPos;
#ifdef FIRE_GRID
layout(location = 2) in float aSurface;
out vec3 vWorld;
flat out int vSurface;
#endif

uniform mat4 uLightViewProj;

void main() {
#ifdef FIRE_GRID
    vWorld = aPos;
    vSurface = int(aSurface + 0.5);
#endif
    gl_Position = uLightViewProj * vec4(aPos, 1.0);
}
