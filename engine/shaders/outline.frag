#version 410 core

in vec2 vUv;

uniform sampler2D uColor;
uniform sampler2D uNormal;
uniform sampler2D uDepth;
uniform sampler2D uAlbedo;
uniform mat4 uView;
uniform vec3 uBias;
uniform vec2 uTexel;
uniform float uDepthThresh;
uniform int uEnable;

layout(location = 0) out vec4 oColor;

vec3 viewNormal(vec2 uv) {
  vec3 n = texture(uNormal, uv).xyz;
  return normalize(mat3(uView) * n);
}

void main() {
  vec3 col = texture(uColor, vUv).rgb;
  float d0 = texture(uDepth, vUv).r;
  float material = texture(uAlbedo, vUv).a;
  if (uEnable == 0 || material > 0.25 || d0 > 200.0) {
    oColor = vec4(col, 1.0);
    return;
  }

  vec3 n0 = viewNormal(vUv);
  vec2 offs[4] = vec2[](
    vec2(uTexel.x, 0.0), vec2(-uTexel.x, 0.0),
    vec2(0.0, uTexel.y), vec2(0.0, -uTexel.y));

  float depthScore = 0.0;
  float normalScore = 0.0;
  float farther = 0.0;
  for (int i = 0; i < 4; ++i) {
    vec2 uv = vUv + offs[i];
    float dn = texture(uDepth, uv).r;
    depthScore += max(dn - d0, 0.0);
    if (d0 - dn > uDepthThresh * 0.4) farther += 1.0;
    if (dn < 200.0) {
      vec3 nn = viewNormal(uv);
      normalScore += max(dot(n0 - nn, uBias), 0.0);
    }
  }

  bool depthEdge = depthScore > uDepthThresh;
  bool normalEdge = normalScore > 0.75 && farther < 0.5 && !depthEdge;

  vec3 albedo = texture(uAlbedo, vUv).rgb;
  float lumaC = dot(col, vec3(0.30, 0.50, 0.20));
  float lumaA = max(dot(albedo, vec3(0.30, 0.50, 0.20)), 0.04);
  float lightLevel = clamp(lumaC / lumaA, 0.0, 1.3);

  if (depthEdge) {
    float darken = mix(0.34, 0.58, clamp(lightLevel, 0.0, 1.0));
    col *= darken;
  }
  if (normalEdge) {
    float boost = mix(1.0, 1.26, clamp(lightLevel, 0.0, 1.0));
    col = col * boost + vec3(0.035, 0.03, 0.015) * clamp(lightLevel, 0.0, 1.0);
  }
  oColor = vec4(col, 1.0);
}
