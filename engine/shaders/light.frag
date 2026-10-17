#version 410 core

// #include "common.glsl"

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
uniform vec3 uSunColor;
uniform vec3 uShadowTint;
uniform vec3 uSkyColor;
uniform float uFocusDepth;
uniform float uFogNear;
uniform float uFogFar;
uniform float uFogMax;
uniform float uNormalBias;
uniform float uDepthBias;
uniform vec2 uCloudScroll;
uniform float uCloudMorph;

layout(location = 0) out vec4 oColor;

void main() {
  float depth = texture(uDepth, vUv).r;
  if (depth > 200.0) {
    oColor = vec4(uSkyColor, 0.0);
    return;
  }

  vec2 ndc = vUv * 2.0 - 1.0;
  vec3 world = uEye + uRight * ndc.x * uHalfW + uUp * ndc.y * uHalfH + uForward * depth;

  vec4 albedoS = texture(uAlbedo, vUv);
  vec4 nrm = texture(uNormal, vUv);
  vec3 n = normalize(nrm.xyz);
  vec3 albedo = albedoS.rgb;
  float material = albedoS.a;
  float ndl = dot(n, -uLightDir);
  float cloud = cloudBand(world, uLightDir, uCloudScroll, uCloudMorph);

  vec3 col;
  float litMask;
  if (material > 0.9) {
    // Canopy mesh. It is the shadow caster, so it does not sample the map.
    // Cloud is evaluated here, per texel, on the same function as the ground.
    vec4 s = shadeCanopy(albedo, n, ndl, uSunColor, uShadowTint, cloud);
    col = s.rgb;
    litMask = s.a;
  } else if (material > 0.6) {
    // Leaf sprite. normal.w is the cloud band at the leaf, so the card is one colour.
    vec4 s = shadeCanopy(albedo, n, ndl, uSunColor, uShadowTint, nrm.w);
    col = s.rgb;
    litMask = s.a;
  } else if (material > 0.25) {
    // Grass. normal.w is the band at the base (0 shadow, 0.5 mid, 1 lit).
    col = shadeBand(albedo, n, uSunColor, uShadowTint, nrm.w);
    litMask = nrm.w;
  } else {
    float sunVis = shadowFactor(uShadow, uLightViewProj, world, n, uLightDir, uNormalBias, uDepthBias);
    float band = (sunVis < 0.5 || ndl <= 0.02) ? 0.0 : cloud;
    col = shadeBand(albedo, n, uSunColor, uShadowTint, band);
    litMask = band;
  }

  if (uFogMax > 0.001) {
    float fog = smoothstep(uFogNear, uFogFar, depth - uFocusDepth);
    col = mix(col, uSkyColor, clamp(fog, 0.0, uFogMax));
  }
  oColor = vec4(col, litMask);
}
