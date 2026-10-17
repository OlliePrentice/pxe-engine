#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 3) in float aSurface;

uniform mat4 uLightViewProj;
uniform float uTime;

void main() {
  vec3 pos = aPos;
  if (aSurface > 3.5) {
    float s = sin(uTime * 0.65 + aPos.x * 0.31 + aPos.z * 0.22) * 0.045;
    pos.x += s;
    pos.z += s * 0.55;
  }
  gl_Position = uLightViewProj * vec4(pos, 1.0);
}
