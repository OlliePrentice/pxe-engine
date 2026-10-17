#version 410 core

in vec2 vUv;

uniform sampler2D uTex;
uniform int uMode;      // 0 filtered colour, 1 nearest colour, 2 normals, 3 depth
uniform int uWeather;   // 2 storm, 3 rain: grade + streaks
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

  if ((uWeather == 2 || uWeather == 3) && (uMode == 0 || uMode == 1)) {
    float storm = uWeather == 2 ? 1.0 : 0.0;
    float l = dot(col, vec3(0.30, 0.50, 0.20));
    col = mix(col, vec3(l), mix(0.28, 0.48, storm));
    col = mix(col, vec3(0.34, 0.44, 0.54), mix(0.22, 0.42, storm));
    col *= mix(vec3(0.88, 0.94, 1.04), vec3(0.74, 0.84, 1.02), storm);
    vec2 texSize = vec2(textureSize(uTex, 0));
    vec2 px = floor(vUv * texSize);
    float column = fract(sin(px.x * 47.11) * 917.13);
    float speed = mix(0.70, 1.35, storm);
    float y = fract(px.y * 0.02 + uTime * speed + column * 4.0);
    float streak = step(mix(0.955, 0.93, storm), column) * step(y, mix(0.14, 0.22, storm));
    float fleck = step(0.988, fract(sin(dot(px, vec2(19.1, 71.7)) + floor(uTime * 2.0)) * 13.7));
    col += (streak * 0.55 + fleck * 0.22) * vec3(0.82, 0.88, 0.92);
  }

  oColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
