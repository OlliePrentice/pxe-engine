#version 410 core

in float vLinearDepth;

uniform sampler2D uDepth;
uniform vec2 uInvRes;
uniform float uShore;
uniform float uTime;

out vec4 oColor;

float hash12(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
  vec2 uv = gl_FragCoord.xy * uInvRes;
  float sceneD = texture(uDepth, uv).r;
  float diff = sceneD - vLinearDepth;

  vec3 shallowCol = vec3(0.50, 0.64, 0.56);
  vec3 deepCol = vec3(0.26, 0.40, 0.42);
  float t = clamp(diff / 1.6, 0.0, 1.0);
  vec3 col = mix(shallowCol, deepCol, t);

  vec2 px = floor(gl_FragCoord.xy);
  float spark = step(0.92, hash12(px + floor(uTime * 1.5)));
  col += spark * 0.08;

  float alpha = mix(0.42, 0.80, t);
  if (diff > 0.04 && diff < uShore) {
    col = mix(col, vec3(0.84, 0.90, 0.78), 0.72);
    alpha = 0.9;
  }
  oColor = vec4(col, alpha);
}
