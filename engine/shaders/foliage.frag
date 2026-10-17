#version 410 core

// #include "common.glsl"

in vec3 vWorld;
in vec3 vBase;
in vec3 vNormal;
in vec3 vColor;
in float vSurface;
in float vLinearDepth;
in vec2 vUv;
in float vKind;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oDepth;

void main() {
  vec2 q = vUv * 2.0 - 1.0;
  if (vKind < 0.5) {
    // Tapered tuft with a hard, slightly jagged edge.
    float y = vUv.y;
    float halfW = mix(0.92, 0.18, y);
    float jag = hash12(floor(vUv * vec2(5.0, 7.0) + vBase.xz)) - 0.5;
    halfW += jag * 0.16;
    if (abs(q.x) > halfW) discard;
    if (y < 0.02) discard;
  } else if (vKind < 2.5) {
    // Four-petal flower, hard pixel edge.
    float ang = atan(q.y, q.x);
    float petal = 0.28 + 0.62 * pow(abs(cos(ang * 2.0)), 0.45);
    if (vKind > 1.5) petal = 0.42 + 0.28 * pow(abs(cos(ang * 2.0)), 0.8);
    if (length(q) > petal) discard;
  } else {
    float jag = hash12(floor(vUv * 6.0 + vBase.xy));
    float r = dot(q, q);
    if (r > 0.62 + jag * 0.34) discard;
    // A bite so clumps don't read as perfect discs.
    if (q.x > 0.35 && q.y < -0.15 && r > 0.25) discard;
  }

  vec3 n = normalize(vNormal);
  vec3 albedo = shadeAlbedo(vColor, n, vBase, vBase, vSurface);
  if (vKind > 0.5 && vKind < 2.5) albedo = vColor;

  // 0 solid (outlines + shadow), 0.5 grass (shadow, no outline), 1 leaf (neither).
  float material = 0.0;
  if (vKind < 2.5) material = 0.5;
  else material = 1.0;

  oAlbedo = vec4(albedo, material);
  oNormal = vec4(n, 0.0);
  oDepth = vec4(vLinearDepth, 0.0, 0.0, 1.0);
}
