#version 410 core

in vec2 vUv;

uniform sampler2D uDepth;
uniform sampler2D uShadow;
uniform mat4 uLightViewProj;
uniform vec3 uEye;
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec3 uForward;
uniform float uHalfW;
uniform float uHalfH;
uniform float uStrength;
uniform float uRayLen;
uniform vec3 uRayColor;

layout(location = 0) out vec4 oColor;

bool sunlit(vec3 world) {
  vec4 lc = uLightViewProj * vec4(world, 1.0);
  vec3 p = lc.xyz / lc.w;
  vec2 suv = p.xy * 0.5 + 0.5;
  if (suv.x <= 0.0 || suv.x >= 1.0 || suv.y <= 0.0 || suv.y >= 1.0) return false;
  float stored = texture(uShadow, suv).r;
  float z = p.z * 0.5 + 0.5;
  return z <= stored + 0.006;
}

void main() {
  if (uStrength <= 0.001) {
    oColor = vec4(0.0);
    return;
  }
  float d = texture(uDepth, vUv).r;
  float endD = d > 200.0 ? 32.0 : d;
  float startD = max(endD - uRayLen, 1.5);
  vec2 ndc = vUv * 2.0 - 1.0;
  vec3 origin = uEye + uRight * ndc.x * uHalfW + uUp * ndc.y * uHalfH;

  const int kSteps = 12;
  float acc = 0.0;
  for (int i = 0; i < kSteps; ++i) {
    float t = (float(i) + 0.5) / float(kSteps);
    float dist = mix(startD, endD, t);
    if (sunlit(origin + uForward * dist)) acc += 1.0;
  }
  float a = acc / float(kSteps);
  // Open air is fully lit and would just fog the frame. Keep the add for the
  // bright part of a shaft, with hard steps so the edge stays a pixel edge.
  a = smoothstep(0.25, 0.92, a);
  a = floor(a * 4.0 + 0.2) / 4.0;
  oColor = vec4(uRayColor * a * uStrength, 1.0);
}
