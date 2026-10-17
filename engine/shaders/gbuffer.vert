#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in float aSurface;

uniform mat4 uViewProj;
uniform mat4 uView;
uniform float uTime;

out vec3 vWorld;
out vec3 vBase;
out vec3 vNormal;
out vec3 vColor;
out float vSurface;
out float vLinearDepth;

void main() {
  vec3 pos = aPos;
  // Whole canopy, one motion. The shadow pass uses the same displacement.
  if (aSurface > 3.5) {
    float s = sin(uTime * 0.65 + aPos.x * 0.31 + aPos.z * 0.22) * 0.045;
    pos.x += s;
    pos.z += s * 0.55;
  }
  vWorld = pos;
  vBase = aPos;
  vNormal = aNormal;
  vColor = aColor;
  vSurface = aSurface;
  vec4 view = uView * vec4(pos, 1.0);
  vLinearDepth = -view.z;
  gl_Position = uViewProj * vec4(pos, 1.0);
}
