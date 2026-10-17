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

// Five octaves. The first sets blobs of roughly 80–220 px; the rest
// break the edge so it is not one smooth oval.
float fbm(vec2 p) {
  float n = 0.0;
  float a = 0.42;
  float w = 0.0;
  for (int i = 0; i < 5; ++i) {
    n += a * valueNoise(p);
    w += a;
    p = p * 2.03 + vec2(1.7, 9.2);
    a *= 0.62;
  }
  return n / w;
}

// 0 shadow, 0.5 mid, 1 lit.
// The sample is pushed along the sun, then mixed with a flat-relief
// sample so a vertical face does not slice the shadow.
float cloudBand(vec3 world, vec3 lightDir, vec2 scroll, float morph) {
  vec3 L = normalize(lightDir);
  vec2 slope = L.xz / max(abs(L.y), 0.22);
  vec2 flatP = world.xz + slope * 0.90;
  vec2 physP = world.xz + slope * world.y;
  vec2 p = mix(flatP, physP, 0.35);
  float n = fbm(p * 0.26 + vec2(9.0, 0.4) + scroll + vec2(morph, -morph * 0.45));
  if (n < 0.455) return 0.0;
  if (n < 0.495) return 0.5;
  return 1.0;
}

// Three hard steps. Grass and stone do not share one shadow ratio:
// grass shadow is teal, stone shadow is a grey. `band` is 0, 0.5, or 1.
vec3 shadeBand(vec3 albedo, vec3 n, vec3 sunColor, vec3 shadowTint, float band) {
  float sat = max(albedo.r, max(albedo.g, albedo.b)) - min(albedo.r, min(albedo.g, albedo.b));
  bool green = albedo.g > albedo.r * 1.02 && albedo.g > albedo.b * 1.15;
  bool stone = !green && sat < 0.20;
  vec3 alb = stone ? albedo * vec3(1.08, 1.12, 1.18) : albedo;
  vec3 tint = green ? shadowTint * vec3(1.056, 1.119, 1.507) : shadowTint;
  vec3 midMul = green ? vec3(0.779, 0.873, 0.780) : vec3(0.78, 0.80, 0.86);
  vec3 hi = alb * sunColor;
  vec3 mid = alb * sunColor * midMul;
  float amb = clamp(n.y * 0.10 + 0.92, 0.90, 1.0);
  vec3 lo = alb * tint * amb;
  if (band > 0.75) return hi;
  if (band > 0.25) return mid;
  return lo;
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

// rgb colour, a = cel band (1 lit, 0.55 mid, 0 shadow) for the outline pass.
vec4 shadeCanopy(vec3 albedo, vec3 n, float ndl, vec3 sunColor, vec3 shadowTint, float cloud) {
  float band;
  if (cloud < 0.25) band = 0.0;
  else if (cloud < 0.75) band = 0.5;
  else {
    float w = clamp(ndl * 0.5 + 0.5, 0.0, 0.999);
    band = w > 0.60 ? 1.0 : (w > 0.36 ? 0.5 : 0.0);
  }
  float mask = band > 0.75 ? 1.0 : (band > 0.25 ? 0.55 : 0.0);
  return vec4(shadeBand(albedo, n, sunColor, shadowTint, band), mask);
}
