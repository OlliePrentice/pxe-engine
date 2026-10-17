float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

// surface: 0 ground, 1 dirt, 2 stone, 3 bark, 4 leaf
vec3 shadeAlbedo(vec3 base, vec3 n, vec3 world, vec3 basePos, float surface) {
  vec3 albedo = base;
  vec2 cell = floor(basePos.xz * 2.0);
  float nse = hash12(cell);

  if (surface < 0.5) {
    albedo *= 0.90 + 0.18 * nse;
    float clump = step(0.82, hash12(floor(basePos.xz * 0.65)));
    albedo = mix(albedo, albedo * vec3(0.86, 0.95, 0.72), clump * 0.35);
  } else if (surface < 1.5) {
    albedo *= 0.88 + 0.20 * nse;
    vec2 g = world.xz;
    float row = step(0.86, fract(g.y * 1.6 + floor(g.x) * 0.37));
    float col = step(0.90, fract(g.x * 1.3));
    float crack = clamp(row + col, 0.0, 1.0);
    albedo *= 1.0 - 0.28 * crack;
    float pebble = step(0.78, hash12(floor(g * 3.0)));
    albedo = mix(albedo, albedo * vec3(0.78, 0.74, 0.68), pebble * 0.45);
  } else if (surface < 2.5) {
    albedo *= 0.92 + 0.14 * hash12(floor(world.xz * 1.4 + world.y));
    float brickX = step(0.88, fract(world.x * 1.25 + floor(world.y * 2.2) * 0.5));
    float brickY = step(0.90, fract(world.y * 2.2));
    float brickZ = step(0.88, fract(world.z * 1.25 + floor(world.y * 2.2) * 0.5));
    float onWall = 1.0 - smoothstep(0.45, 0.85, abs(n.y));
    float mortar = clamp((brickX + brickZ) * onWall + brickY, 0.0, 1.0);
    albedo *= 1.0 - 0.22 * mortar;
    float moss = smoothstep(0.55, 0.98, n.y);
    moss *= 0.35 + 0.65 * step(0.42, hash12(floor(world.xz * 1.7 + 3.1)));
    albedo = mix(albedo, vec3(0.42, 0.52, 0.30), moss * 0.78);
  } else if (surface < 3.5) {
    albedo *= 0.86 + 0.22 * hash12(floor(world.xy * 3.0 + world.z));
  } else {
    albedo *= 0.90 + 0.18 * nse;
  }
  return albedo;
}
