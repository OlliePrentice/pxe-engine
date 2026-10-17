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
uniform vec3 uSunColor;

layout(location = 0) out vec4 oColor;

vec3 viewNormal(vec2 uv) {
  vec3 n = texture(uNormal, uv).xyz;
  return normalize(mat3(uView) * n);
}

void main() {
  vec4 src = texture(uColor, vUv);
  vec3 col = src.rgb;
  float d0 = texture(uDepth, vUv).r;
  float material = texture(uAlbedo, vUv).a;
  // Grass and leaves opt out. Their silhouette is the sprite itself.
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
    if (d0 - dn > uDepthThresh * 0.35) farther += 1.0;
    if (dn < 200.0) {
      vec3 nn = viewNormal(uv);
      normalScore += max(dot(n0 - nn, uBias), 0.0);
    }
  }

  // Normal edges are for hard creases (a step lip, a block corner), not
  // the gentle change of a hill. Depth edges are silhouettes.
  bool depthEdge = depthScore > uDepthThresh;
  bool normalEdge = normalScore > 0.95 && farther < 0.5 && !depthEdge;
  if (!depthEdge && !normalEdge) {
    oColor = vec4(col, 1.0);
    return;
  }

  // Alpha of the light buffer is the cel band (1 lit, 0 shadow).
  float lit = src.a;
  vec3 albedo = texture(uAlbedo, vUv).rgb;
  if (lit > 0.72) {
    // Light-facing lip: brighter than the lit surface, tinted by the sun.
    vec3 hi = albedo * uSunColor * 1.42 + vec3(0.07, 0.06, 0.04);
    float amt = normalEdge ? 0.88 : 0.62;
    col = mix(col, clamp(hi, 0.0, 1.0), amt);
  } else {
    // Shadowed silhouette. Stay in the local colour, just darker.
    col *= normalEdge ? 0.78 : 0.66;
  }
  oColor = vec4(col, 1.0);
}
