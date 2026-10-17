#version 410 core

flat in vec3 vColor;
flat in vec3 vNormal;
flat in float vGate;
flat in float vMaterial;
flat in float vKind;
smooth in vec2 vUv;
in float vLinearDepth;

uniform sampler2D uGrass;
uniform sampler2D uFlower;
uniform sampler2D uLeaf;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oDepth;

void main() {
  vec4 tex;
  if (vKind < 0.5) tex = texture(uGrass, vUv);
  else if (vKind < 1.5) tex = texture(uFlower, vUv);
  else tex = texture(uLeaf, vUv);
  if (tex.a < 0.5) discard;

  // Unlit albedo. The light pass shades it with the same function as the ground,
  // using the gate and the terrain (or canopy) normal, so a tuft matches its base.
  oAlbedo = vec4(vColor, vMaterial);
  oNormal = vec4(normalize(vNormal), vGate);
  oDepth = vec4(vLinearDepth, 0.0, 0.0, 1.0);
}
