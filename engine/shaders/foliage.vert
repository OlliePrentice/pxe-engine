#version 410 core

layout(location = 0) in vec2 aUv;
layout(location = 4) in vec3 iPos;
layout(location = 5) in vec3 iNormal;
layout(location = 6) in vec3 iColor;
layout(location = 7) in vec4 iParams;  // phase, scale, kind, unused

uniform mat4 uViewProj;
uniform mat4 uView;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform float uTime;

out vec3 vWorld;
out vec3 vBase;
out vec3 vNormal;
out vec3 vColor;
out float vSurface;
out float vLinearDepth;
out vec2 vUv;
out float vKind;

void main() {
  float phase = iParams.x;
  float scale = iParams.y;
  float kind = iParams.z;
  vKind = kind;
  vUv = aUv;
  vBase = iPos;
  vNormal = normalize(iNormal);
  vColor = iColor;

  vec3 pos;
  if (kind < 2.5) {
    // Grass and flowers: vertical billboards, rooted, swaying from the tip.
    float height = (kind < 0.5 ? 0.40 : 0.34) * scale;
    float width = (kind < 0.5 ? 0.26 : 0.36) * scale;
    vec2 c = vec2(aUv.x - 0.5, aUv.y);
    float sway = sin(uTime * 1.55 + phase) * c.y * c.y;
    pos = iPos + uCamRight * (c.x + sway * 0.22) * width + vec3(0.0, c.y * height, 0.0);
    vSurface = 0.0;  // shaded as the ground they grow from
    vKind = kind;
  } else {
    float size = 0.55 * scale;
    vec2 c = aUv - 0.5;
    float sway = sin(uTime * 1.15 + phase) * 0.05 * scale;
    pos = iPos + (uCamRight * c.x + uCamUp * c.y) * size + uCamRight * sway;
    vSurface = 4.0;
  }

  vWorld = pos;
  vec4 view = uView * vec4(pos, 1.0);
  vLinearDepth = -view.z;
  gl_Position = uViewProj * vec4(pos, 1.0);
}
