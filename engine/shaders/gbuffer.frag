#version 410 core

// #include "common.glsl"

in vec3 vWorld;
in vec3 vBase;
in vec3 vNormal;
in vec3 vColor;
in float vSurface;
in float vLinearDepth;

layout(location = 0) out vec4 oAlbedo;     // rgb colour, a = material (0 solid, 0.5 grass, 1 leaf)
layout(location = 1) out vec4 oNormal;     // world normal
layout(location = 2) out vec4 oDepth;      // linear view depth in r

void main() {
  vec3 n = normalize(vNormal);
  vec3 albedo = shadeAlbedo(vColor, n, vWorld, vBase, vSurface);
  // Canopies are lit from the smooth normal and must not sample their own shadow.
  float material = vSurface > 3.5 ? 1.0 : 0.0;
  oAlbedo = vec4(albedo, material);
  oNormal = vec4(n, 0.0);
  oDepth = vec4(vLinearDepth, 0.0, 0.0, 1.0);
}
