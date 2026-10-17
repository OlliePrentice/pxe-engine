#version 410 core

// #include "common.glsl"

// Grass and flowers: cylindrical billboard, pivot at the bottom centre.
// Leaves: camera-facing card, pivot at the centre.
// Wind never shears or rotates the quad. It picks a flipbook cell.

layout(location = 0) in vec2 aUv;
layout(location = 4) in vec3 iPos;
layout(location = 5) in vec3 iNormal;
layout(location = 6) in vec3 iColor;
layout(location = 7) in vec4 iParams;  // phase, scale, kind, variant

uniform mat4 uViewProj;
uniform mat4 uView;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform float uTime;
uniform float uTexelWorld;

uniform vec2 uWindDir;
uniform float uWindSpeed;
uniform float uWindStrength;
uniform float uWindGust;
uniform float uWindFps;
uniform float uRain;

uniform sampler2D uShadow;
uniform mat4 uLightViewProj;
uniform vec3 uLightDir;
uniform float uNormalBias;
uniform float uDepthBias;
uniform vec2 uCloudScroll;

flat out vec3 vColor;
flat out vec3 vNormal;
flat out float vGate;
flat out float vMaterial;
flat out float vKind;
smooth out vec2 vUv;
out float vLinearDepth;

void main() {
  float phase = iParams.x;
  float kind = iParams.z;
  float variant = iParams.w;
  vec3 n = normalize(iNormal);
  vec3 base = iPos;
  bool leaf = kind > 1.5;
  bool flower = !leaf && kind > 0.5;

  // Quantised clock so a pose holds for several refreshes, staggered per sprite.
  float fps = max(uWindFps, 1.0);
  float tq = floor((uTime + phase) * fps) / fps;

  float n1 = valueNoise(base.xz * 0.30 - uWindDir * uWindSpeed * tq);
  float n2 = valueNoise(base.xz * 0.17 + vec2(19.4, 7.1) - uWindDir * uWindSpeed * tq * 0.65);
  float g = n1 * 0.62 + n2 * 0.38;
  float halfw = max(0.5 * uWindGust, 0.02);
  float gust = smoothstep(0.5 - halfw, 0.5 + halfw, g);
  float amp = clamp(uWindStrength * mix(0.25, 1.0, gust), 0.0, 0.999);

  int frameCount = leaf ? 6 : (flower ? 4 : 8);
  int bandCount = flower ? 2 : 3;
  int shape = int(clamp(variant + 0.5, 0.0, 1.0));
  int band = clamp(int(amp * float(bandCount)), 0, bandCount - 1);

  float travel = fract(dot(base.xz, uWindDir) * 0.23 - tq * uWindSpeed * 0.23 + phase * 0.37);
  int frame = int(travel * float(frameCount));
  if (band == 0) {
    frame = 0;
    // Rare idle twitch on grass only, and only once the wind is up a little.
    if (!flower && !leaf && uWindStrength >= 0.16 && fract(phase * 2.7 + tq * 0.17) > 0.94) {
      frame = min(6, frameCount - 1);
    }
  }
  frame = clamp(frame, 0, frameCount - 1);
  int row = shape * bandCount + band;

  vec2 corner = aUv;
  if (dot(uWindDir, vec2(uCamRight.x, uCamRight.z)) < 0.0) corner.x = 1.0 - corner.x;

  vec2 cells = leaf ? vec2(6.0, 6.0) : (flower ? vec2(4.0, 4.0) : vec2(8.0, 6.0));
  float quadPx = leaf ? 10.0 : 12.0;
  vUv = (vec2(float(frame), float(row)) + corner) / cells;

  vec3 pos;
  if (!leaf) {
    vec3 right = vec3(uCamRight.x, 0.0, uCamRight.z);
    float rl = length(right);
    right = rl > 1e-4 ? right / rl : vec3(1.0, 0.0, 0.0);
    vec2 c = vec2(aUv.x - 0.5, aUv.y);
    pos = base + right * c.x * quadPx * uTexelWorld + vec3(0.0, c.y * quadPx * uTexelWorld, 0.0);
  } else {
    vec2 c = aUv - 0.5;
    pos = base + (uCamRight * c.x + uCamUp * c.y) * quadPx * uTexelWorld;
    // Whole-pixel crown sway from the gust at the tree, 0 when calm, at most 2px.
    vec2 cell = floor(base.xz / 3.5 + 0.5);
    float cg = valueNoise(cell * 0.21 - uWindDir * uWindSpeed * tq);
    float px = round((cg - 0.5) * uWindStrength * 4.0);
    float h = clamp(base.y / 3.2, 0.15, 1.0);
    pos.xz += uWindDir * px * uTexelWorld * h;
    // Rain: upward-facing leaves drop a single pixel, on a stepped clock.
    if (n.y > 0.55 && uRain > 0.35) {
      float bob = step(0.62, valueNoise(base.xz * 1.9 + floor(tq * 3.0)));
      pos.y -= bob * uTexelWorld;
    }
  }

  float cloud = cloudShadow(base.xz, uCloudScroll);
  if (leaf) {
    vGate = cloud;
    vMaterial = 0.75;
  } else {
    float sunVis = shadowFactor(uShadow, uLightViewProj, base, n, uLightDir, uNormalBias, uDepthBias);
    float ndl = dot(n, -normalize(uLightDir));
    vGate = (sunVis > 0.5 && cloud > 0.5 && ndl > 0.02) ? 1.0 : 0.0;
    vMaterial = 0.5;
  }

  vColor = iColor;
  vNormal = n;
  vKind = kind;
  vec4 view = uView * vec4(pos, 1.0);
  vLinearDepth = -view.z;
  gl_Position = uViewProj * vec4(pos, 1.0);
}
