#version 410 core

in vec2 vUv;

uniform sampler2D uTex;
uniform int uMode;      // 0 filtered colour, 1 nearest colour, 2 normals, 3 depth
uniform int uWeather;   // 2 storm, 3 rain
uniform float uTime;
uniform float uFlash;
uniform vec3 uSkyColor;

out vec4 oColor;

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
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

float lum(vec3 c) { return dot(c, vec3(0.30, 0.50, 0.20)); }

vec3 filtered(vec2 uv) {
  vec2 texSize = vec2(textureSize(uTex, 0));
  vec2 texel = uv * texSize;
  vec2 boxSize = clamp(fwidth(texel), 1e-5, 1.0);
  vec2 tx = floor(texel - 0.5) + 0.5;
  vec2 f = texel - tx;
  vec2 t = smoothstep(0.5 - boxSize * 0.5, 0.5 + boxSize * 0.5, f);
  vec2 sampleUV = (tx + t) / texSize;
  return textureGrad(uTex, sampleUV, dFdx(uv), dFdy(uv)).rgb;
}

vec3 nearest(vec2 uv) {
  vec2 texSize = vec2(textureSize(uTex, 0));
  return texture(uTex, (floor(uv * texSize) + 0.5) / texSize).rgb;
}

// Daylight hexes onto the rain palette. Grass goes teal, stone goes
// blue-violet, water goes #33668f, and a near-white ripple stays #5c8fb8.
vec3 rainGrade(vec3 c, float storm) {
  float y = max(c.r, max(c.g, c.b));
  float green = smoothstep(0.0, 0.05, c.g - max(c.r, c.b));
  float water = smoothstep(0.02, 0.10, c.b - max(c.r, c.g));
  float white = smoothstep(0.90, 0.98, min(c.r, min(c.g, c.b)));
  float l = lum(c);
  vec3 grass = vec3(l * 0.48, l * 0.68, l * 0.56);
  float stoneK = mix(0.85, 1.0, smoothstep(0.50, 0.82, y));
  vec3 stone = vec3(0.220, 0.290, 0.463) * stoneK;
  float rip = smoothstep(0.93, 0.99, y);
  vec3 wet = mix(vec3(0.200, 0.400, 0.561), vec3(0.361, 0.561, 0.722), rip);
  vec3 graded = mix(stone, grass, green);
  graded = mix(graded, wet, water * (1.0 - green));
  graded = mix(graded, vec3(0.75, 0.86, 0.92) * y, white);
  graded *= mix(1.0, 0.90, storm);
  return graded;
}

vec3 rainLayers(vec3 rgb, vec2 uv, float time, float storm, float rim) {
  vec2 texSize = vec2(textureSize(uTex, 0));
  vec2 px = floor(uv * texSize);
  // Screen rain steps at 15 Hz. Each step is a new set, not a fall.
  // `time` is already quantised by the caller.
  float tick = floor(time * 15.0 + 0.5);

  float colId = floor(px.x + px.y * 0.11);
  float h = hash12(vec2(colId, tick));
  float h2 = hash12(vec2(colId, tick + 19.0));
  float y0 = hash12(vec2(colId + 3.0, tick)) * texSize.y;
  float along = px.y - y0;
  float longStreak = step(mix(0.948, 0.928, storm), h) * step(0.0, along) * step(along, mix(11.0, 28.0, h2));
  float fragH = hash12(vec2(colId + 40.0, tick + 2.0));
  float frag = step(mix(0.90, 0.86, storm), fragH) * step(0.0, along) * step(along, mix(4.0, 9.0, fragH));
  vec3 pale = vec3(0.820, 0.925, 1.0);
  rgb = mix(rgb, pale, clamp(longStreak * 0.25 + frag * 0.16, 0.0, 0.25));

  // Splashes: a 2px cluster, then two or three droplets, then gone. 3 ticks.
  vec2 cell = floor(px / 18.0);
  float period = 10.0;
  float phase = floor(hash12(cell) * 6.0);
  float local = mod(tick + phase, period);
  float spawn = step(mix(0.72, 0.58, storm), hash12(cell + vec2(4.0, 1.0)));
  vec2 d = px - (cell * 18.0 + vec2(8.0, 9.0));
  float cluster = step(length(d), 1.15);
  float split = step(length(d - vec2(-1.6, 0.6)), 0.55) + step(length(d - vec2(1.7, -0.5)), 0.55);
  float spread = step(length(d - vec2(-2.8, 1.1)), 0.55) + step(length(d - vec2(3.0, -1.1)), 0.55) +
                 step(length(d - vec2(0.3, 2.4)), 0.55);
  float splash = spawn * (local < 0.5 ? cluster : (local < 1.5 ? clamp(split, 0.0, 1.0) : (local < 2.5 ? clamp(spread, 0.0, 1.0) : 0.0)));
  vec3 sky = vec3(0.161, 0.306, 0.396);
  float onSky = step(0.96, 1.0 - distance(rgb, sky));
  rgb += splash * (1.0 - onSky) * vec3(0.07, 0.08, 0.05);

  // Dashes along a horizontal rim. `rim` was measured on the daylight image.
  float dash = step(0.70, hash12(vec2(px.x, tick + floor(px.y * 0.17))));
  rgb = mix(rgb, vec3(0.72, 0.84, 0.95), rim * dash * 0.65);

  // Faint drifting brightness, smooth, about ±8/255.
  float mist = valueNoise(px * 0.0065 + vec2(time * 0.010, time * 0.006));
  rgb += (mist - 0.5) * 0.050;
  return rgb;
}

void main() {
  vec3 col = (uMode == 0) ? filtered(vUv) : nearest(vUv);
  if (uMode == 2) col = col * 0.5 + 0.5;
  if (uMode == 3) col = vec3(clamp(col.r / 48.0, 0.0, 1.0));

  if (uMode == 0) {
    col = max(col, vec3(0.012, 0.014, 0.016));
  }

  if ((uWeather == 2 || uWeather == 3) && (uMode == 0 || uMode == 1)) {
    float storm = uWeather == 2 ? 1.0 : 0.0;
    vec2 texSize = vec2(textureSize(uTex, 0));
    vec2 texel = 1.0 / texSize;
    vec3 src = col;
    vec3 below = texture(uTex, vUv - vec2(0.0, texel.y)).rgb;
    vec3 leftC = texture(uTex, vUv - vec2(texel.x, 0.0)).rgb;
    vec3 rightC = texture(uTex, vUv + vec2(texel.x, 0.0)).rgb;
    vec3 leftB = texture(uTex, vUv - vec2(texel.x, texel.y)).rgb;
    vec3 rightB = texture(uTex, vUv + vec2(texel.x, -texel.y)).rgb;
    float rim = step(0.11, lum(src) - lum(below)) * step(0.07, lum(leftC) - lum(leftB)) *
                step(0.07, lum(rightC) - lum(rightB));
    vec3 sky = mix(vec3(0.145, 0.275, 0.365), vec3(0.110, 0.220, 0.320), storm);
    if (distance(col, uSkyColor) < 0.035) {
      col = sky;
    } else {
      col = rainGrade(col, storm);
    }
    // Hold the screen layer for 1/15 s so the ground does not crawl.
    float stepped = floor(uTime * 15.0) / 15.0;
    col = rainLayers(col, vUv, stepped, storm, rim);
    if (uFlash > 0.001) {
      float l = lum(col);
      float up = smoothstep(0.18, 0.42, l);
      vec3 bolt = mix(vec3(0.443, 0.498, 0.843), vec3(0.710, 0.816, 1.0), up);
      col = mix(col, bolt, clamp(uFlash * mix(0.55, 1.20, up), 0.0, 0.92));
      col *= mix(1.0, mix(0.38, 1.0, smoothstep(0.14, 0.36, l)), uFlash);
    }
  }

  oColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
