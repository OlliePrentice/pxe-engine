#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 3) in float aSurface;

uniform mat4 uLightViewProj;

void main() {
  gl_Position = uLightViewProj * vec4(aPos, 1.0);
}
