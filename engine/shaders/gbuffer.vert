#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in float aSurface;

uniform mat4 uViewProj;
uniform mat4 uView;

out vec3 vWorld;
out vec3 vBase;
out vec3 vNormal;
out vec3 vColor;
out float vSurface;
out float vLinearDepth;

void main() {
  vWorld = aPos;
  vBase = aPos;
  vNormal = aNormal;
  vColor = aColor;
  vSurface = aSurface;
  vec4 view = uView * vec4(aPos, 1.0);
  vLinearDepth = -view.z;
  gl_Position = uViewProj * vec4(aPos, 1.0);
}
