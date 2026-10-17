#version 410 core

in vec2 vUv;

uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform sampler2D uDepth;
uniform sampler2D uShadow;
uniform mat4 uLightViewProj;
uniform vec3 uEye;
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec3 uForward;
uniform float uHalfW;
uniform float uHalfH;
uniform vec3 uLightDir;
uniform vec3 uFogColor;
uniform float uFocusDepth;
uniform float uFogNear;
uniform float uFogFar;
uniform float uFogMax;

layout(location = 0) out vec4 oColor;

float shadowFactor(vec3 world, vec3 n) {
  vec4 lc = uLightViewProj * vec4(world, 1.0);
  vec3 p = lc.xyz / lc.w;
  vec2 suv = p.xy * 0.5 + 0.5;
  if (suv.x <= 0.0 || suv.x >= 1.0 || suv.y <= 0.0 || suv.y >= 1.0) return 1.0;
  float stored = texture(uShadow, suv).r;
  float z = p.z * 0.5 + 0.5;
  float facing = clamp(dot(n, -uLightDir), 0.0, 1.0);
  float bias = mix(0.014, 0.0028, facing);
  return z <= stored + bias ? 1.0 : 0.0;
}

void main() {
  float depth = texture(uDepth, vUv).r;
  if (depth > 200.0) {
    oColor = vec4(uFogColor, 1.0);
    return;
  }

  vec2 ndc = vUv * 2.0 - 1.0;
  vec3 world = uEye + uRight * ndc.x * uHalfW + uUp * ndc.y * uHalfH + uForward * depth;

  vec4 albedoS = texture(uAlbedo, vUv);
  vec3 n = normalize(texture(uNormal, vUv).xyz);
  vec3 albedo = albedoS.rgb;
  float material = albedoS.a;

  // Leaves sit inside the canopy shadow proxy, so they are lit from the
  // stored sphere normal and skip the shadow map. Grass and solids receive it.
  float sunVis = material < 0.75 ? shadowFactor(world, n) : 1.0;
  float ndl = dot(n, -uLightDir);

  vec3 col;
  if (sunVis < 0.5) {
    // Soft monochrome Lambert in shadow: contrast without a flat black cutout.
    float amb = clamp(ndl * 0.55 + 0.45, 0.0, 1.0);
    amb = floor(amb * 3.0) / 2.0;
    vec3 tint = vec3(0.42, 0.56, 0.56);
    col = albedo * tint * (0.72 + 0.40 * amb);
  } else {
    float wrapped = clamp(ndl * 0.5 + 0.5, 0.0, 0.999);
    float band = floor(wrapped * 4.0) / 3.0;
    vec3 lo = vec3(0.64, 0.68, 0.56);
    vec3 hi = vec3(1.16, 1.12, 1.06);
    col = albedo * mix(lo, hi, band);
  }

  // Depth is measured from the eye, which sits far back on an ortho camera.
  // Haze should grow past the focal plane, not from the eye itself.
  float fog = smoothstep(uFogNear, uFogFar, depth - uFocusDepth);
  col = mix(col, uFogColor, clamp(fog, 0.0, uFogMax));
  oColor = vec4(col, 1.0);
}
