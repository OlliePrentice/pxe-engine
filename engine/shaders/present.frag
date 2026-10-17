#version 410 core

in vec2 vUv;

uniform sampler2D uTex;
uniform int uMode;      // 0 filtered colour, 1 nearest colour, 2 normals, 3 depth
uniform int uWeather;   // 2 = rain grade + streaks
uniform float uTime;

out vec4 oColor;

vec3 filtered(vec2 uv) {
  vec2 texSize = vec2(textureSize(uTex, 0));
  vec2 texel = uv * texSize;
  // Pixel footprint in texels. Clamped to one texel: this is a magnifying
  // upscale, and mipmaps cover the case where a pixel is larger than that.
  vec2 boxSize = clamp(fwidth(texel), 1e-5, 1.0);
  vec2 tx = floor(texel - 0.5) + 0.5;
  vec2 f = texel - tx;
  // Quadratic window -> cubic transition, which smoothstep evaluates directly.
  // At an integer scale the sample points land outside this band, so the
  // result is a hard pixel. At a fractional scale only the texel edge blends.
  vec2 t = smoothstep(0.5 - boxSize * 0.5, 0.5 + boxSize * 0.5, f);
  vec2 sampleUV = (tx + t) / texSize;
  return textureGrad(uTex, sampleUV, dFdx(uv), dFdy(uv)).rgb;
}

vec3 nearest(vec2 uv) {
  vec2 texSize = vec2(textureSize(uTex, 0));
  return texture(uTex, (floor(uv * texSize) + 0.5) / texSize).rgb;
}

void main() {
  vec3 col = (uMode == 0) ? filtered(vUv) : nearest(vUv);
  if (uMode == 2) col = col * 0.5 + 0.5;
  if (uMode == 3) col = vec3(clamp(col.r / 48.0, 0.0, 1.0));

  if (uMode == 0) {
    // Shadows are tinted, so this only catches a true zero from an empty pixel.
    col = max(col, vec3(0.012, 0.014, 0.016));
  }

  if (uWeather == 2 && (uMode == 0 || uMode == 1)) {
    float l = dot(col, vec3(0.30, 0.50, 0.20));
    col = mix(col, vec3(l), 0.42);
    col = mix(col, vec3(0.32, 0.42, 0.52), 0.40);
    col *= vec3(0.80, 0.90, 1.06);
    vec2 texSize = vec2(textureSize(uTex, 0));
    vec2 px = floor(vUv * texSize);
    float column = fract(sin(px.x * 47.11) * 917.13);
    float y = fract(px.y * 0.02 + uTime * 0.9 + column * 4.0);
    float streak = step(0.945, column) * step(y, 0.16);
    float fleck = step(0.988, fract(sin(dot(px, vec2(19.1, 71.7)) + floor(uTime * 2.0)) * 13.7));
    col += (streak * 0.50 + fleck * 0.28) * vec3(0.82, 0.88, 0.92);
  }

  oColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
