float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

// Flat, textureless colour. Banding comes from the lighting pass, not from
// per-texel noise. surface is kept so callers can still branch later.
vec3 shadeAlbedo(vec3 base, vec3 n, vec3 world, vec3 basePos, float surface) {
  return base + (n.x + world.x + basePos.x + surface) * 0.0;
}
