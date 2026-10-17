#version 410 core

in float vLinearDepth;

uniform sampler2D uDepth;
uniform vec2 uInvRes;
uniform float uPixelDepth;

out vec4 oColor;

void main() {
  vec2 uv = gl_FragCoord.xy * uInvRes;
  float sceneD = texture(uDepth, uv).r;
  float diff = sceneD - vLinearDepth;

  // The bank pixel is terrain closer than the water plane. Only the water
  // texel touching that bank passes, which keeps the line one pixel wide.
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
  oColor = vec4(col, alpha);
}
