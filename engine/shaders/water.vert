#version 410 core

layout(location = 0) in vec3 aPos;

uniform mat4 uViewProj;
uniform mat4 uView;

out float vLinearDepth;
out vec3 vWorld;

void main() {
  vWorld = aPos;
  vec4 view = uView * vec4(aPos, 1.0);
  vLinearDepth = -view.z;
  gl_Position = uViewProj * vec4(aPos, 1.0);
}
