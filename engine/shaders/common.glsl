float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

// Flat, textureless colour. Banding comes from the lighting pass.
vec3 shadeAlbedo(vec3 base, vec3 n, vec3 world, vec3 basePos, float surface) {
  return base + (n.x + world.x + basePos.x + surface) * 0.0;
}

float valueNoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash12(i);
  float b = hash12(i + vec2(1.0, 0.0));
  float c = hash12(i + vec2(0.0, 1.0));
  float d = hash12(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// 1 in the open, 0 under a cloud. One hard threshold, large features,
// so most of the meadow stays a single quiet band.
float cloudShadow(vec2 xz, vec2 scroll) {
  // Offset picked so the default view holds one cloud, not a blanket.
  float n = valueNoise(xz * 0.05 + vec2(3.0, 4.0) - scroll);
  return n > 0.78 ? 0.0 : 1.0;
}

// Manual compare so the vertex and fragment stages share one function.
// Sampler is an argument: the include does not force a uniform on every shader.
float shadowFactor(sampler2D shadowMap, mat4 lightVP, vec3 world, vec3 n, vec3 lightDir,
                   float normalBias, float depthBias) {
  vec3 lit = -normalize(lightDir);
  float nd = clamp(dot(n, lit), 0.0, 1.0);
  vec3 pworld = world + n * normalBias * (1.0 - nd * 0.85) + lit * (normalBias * 0.35);
  vec4 lc = lightVP * vec4(pworld, 1.0);
  vec3 p = lc.xyz / lc.w;
  vec2 suv = p.xy * 0.5 + 0.5;
  if (suv.x <= 0.0 || suv.x >= 1.0 || suv.y <= 0.0 || suv.y >= 1.0) return 1.0;
  float stored = texture(shadowMap, suv).r;
  float z = p.z * 0.5 + 0.5;
  float bias = depthBias * mix(2.4, 0.35, nd);
  return z <= stored + bias ? 1.0 : 0.0;
}

vec3 shadeGround(vec3 albedo, vec3 n, vec3 sunColor, vec3 shadowTint, float lit) {
  vec3 hi = albedo * sunColor;
  float amb = clamp(n.y * 0.10 + 0.92, 0.90, 1.0);
  vec3 lo = albedo * shadowTint * amb;
  return mix(lo, hi, step(0.5, lit));
}

// rgb colour, a = cel band (1 lit, 0.55 mid, 0 shadow) for the outline pass.
vec4 shadeCanopy(vec3 albedo, vec3 n, float ndl, vec3 sunColor, vec3 shadowTint, float cloud) {
  vec3 hi = albedo * sunColor;
  float amb = clamp(n.y * 0.10 + 0.92, 0.90, 1.0);
  vec3 lo = albedo * shadowTint * amb;
  float w = clamp(ndl * 0.5 + 0.5, 0.0, 0.999);
  if (cloud < 0.5 || w <= 0.36) return vec4(lo, 0.0);
  if (w > 0.60) return vec4(hi, 1.0);
  return vec4(albedo * mix(shadowTint, sunColor, 0.58), 0.55);
}
