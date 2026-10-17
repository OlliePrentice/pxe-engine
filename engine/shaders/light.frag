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
uniform vec3 uSunColor;
uniform vec3 uShadowTint;
uniform vec3 uSkyColor;
uniform float uFocusDepth;
uniform float uFogNear;
uniform float uFogFar;
uniform float uFogMax;
uniform float uNormalBias;
uniform float uDepthBias;

layout(location = 0) out vec4 oColor;

float shadowFactor(vec3 world, vec3 n) {
  vec3 lit = -normalize(uLightDir);
  float nd = clamp(dot(n, lit), 0.0, 1.0);
  // Lift the sample off the surface so a nearest-filtered map does not speckle.
  vec3 pworld = world + n * uNormalBias * (1.0 - nd * 0.85) + lit * (uNormalBias * 0.35);
  vec4 lc = uLightViewProj * vec4(pworld, 1.0);
  vec3 p = lc.xyz / lc.w;
  vec2 suv = p.xy * 0.5 + 0.5;
  if (suv.x <= 0.0 || suv.x >= 1.0 || suv.y <= 0.0 || suv.y >= 1.0) return 1.0;
  float stored = texture(uShadow, suv).r;
  float z = p.z * 0.5 + 0.5;
  float bias = uDepthBias * mix(2.4, 0.35, nd);
  return z <= stored + bias ? 1.0 : 0.0;
}

void main() {
  float depth = texture(uDepth, vUv).r;
  if (depth > 200.0) {
    oColor = vec4(uSkyColor, 0.0);
    return;
  }

  vec2 ndc = vUv * 2.0 - 1.0;
  vec3 world = uEye + uRight * ndc.x * uHalfW + uUp * ndc.y * uHalfH + uForward * depth;

  vec4 albedoS = texture(uAlbedo, vUv);
  vec3 n = normalize(texture(uNormal, vUv).xyz);
  vec3 albedo = albedoS.rgb;
  float material = albedoS.a;
  float ndl = dot(n, -uLightDir);

  vec3 hi = albedo * uSunColor;
  vec3 lo = albedo * uShadowTint;
  // Soft monochrome form inside the shadow band, so a shaded stone face
  // still has a top and a side. Never a third colour ramp on the ground.
  float amb = clamp(n.y * 0.10 + 0.92, 0.90, 1.0);
  lo *= amb;

  float litMask;
  vec3 col;
  if (material > 0.75) {
    // Canopy: the mesh normal is the smooth enclosing shape, so a few hard
    // bands read as one blob instead of a pile of leaves.
    float w = clamp(ndl * 0.5 + 0.5, 0.0, 0.999);
    if (w > 0.60) {
      col = hi;
      litMask = 1.0;
    } else if (w > 0.36) {
      col = albedo * mix(uShadowTint, uSunColor, 0.58);
      litMask = 0.55;
    } else {
      col = lo;
      litMask = 0.0;
    }
  } else {
    float sunVis = shadowFactor(world, n);
    litMask = (sunVis > 0.5 && ndl > 0.02) ? 1.0 : 0.0;
    col = mix(lo, hi, litMask);
  }

  if (uFogMax > 0.001) {
    float fog = smoothstep(uFogNear, uFogFar, depth - uFocusDepth);
    col = mix(col, uSkyColor, clamp(fog, 0.0, uFogMax));
  }
  oColor = vec4(col, litMask);
}
