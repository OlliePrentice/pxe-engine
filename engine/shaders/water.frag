#version 410 core

in float vLinearDepth;
in vec3 vWorld;

uniform sampler2D uDepth;
uniform vec2 uInvRes;
uniform float uPixelDepth;
uniform float uTime;
uniform int uWeather;
uniform float uTexelWorld;

out vec4 oColor;

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

void main() {
  vec2 uv = gl_FragCoord.xy * uInvRes;
  float sceneD = texture(uDepth, uv).r;
  float diff = sceneD - vLinearDepth;

  bool shore = false;
  vec2 offs[4] = vec2[](
    vec2(uInvRes.x, 0.0), vec2(-uInvRes.x, 0.0),
    vec2(0.0, uInvRes.y), vec2(0.0, -uInvRes.y));
  for (int i = 0; i < 4; ++i) {
    float nd = texture(uDepth, uv + offs[i]).r;
    if (nd + uPixelDepth * 0.15 < vLinearDepth) shore = true;
  }
  if (shore && diff < uPixelDepth * 8.0) {
    oColor = vec4(0.96, 0.98, 0.97, 1.0);
    return;
  }

  float deep = clamp(diff / 1.4, 0.0, 1.0);
  vec3 col = mix(vec3(0.62, 0.82, 0.90), vec3(0.36, 0.64, 0.78), deep);
  float alpha = mix(0.58, 0.82, deep);

  // Ripples live on the water, at 30 Hz. A ring grows for about a quarter
  // second, then breaks into dashes. Rain water is nearly opaque so the
  // grade sees the water colour, not the bed under it. The ring is a
  // light blue, which the grade lifts to the ripple hex.
  if (uWeather >= 2) {
    alpha = 1.0;
    col = vec3(0.55, 0.78, 0.90);
    float tick = floor(uTime * 30.0);
    vec2 cell = floor(vWorld.xz / 0.62);
    float h = hash12(cell);
    float local = mod(tick + floor(h * 6.0), 14.0);
    if (h > 0.72 && local < 8.0) {
      vec2 center = (cell + 0.5) * 0.62;
      vec2 q = (vWorld.xz - center) / max(uTexelWorld, 1e-4);
      float k = local / 7.0;
      vec2 rad = vec2(mix(3.5, 6.0, k), mix(2.0, 2.6, k));
      float e = length(q / rad);
      float ring = 1.0 - smoothstep(0.0, 0.75, abs(e - 1.0) * rad.y);
      if (local > 5.0) ring *= step(0.45, hash12(floor(q) + local));
      col = mix(col, vec3(0.78, 0.90, 1.0), clamp(ring, 0.0, 1.0));
    }
  }

  oColor = vec4(col, alpha);
}
