// Shared toon ramp. Spliced into the mesh fragment shader and the grass
// vertex shader so both run the same function on the same kind of inputs.
const int kSurfaces = 7;
const int kSurfStone = 2;
const int kSurfWater = 5;
const int kSurfFoam = 6;

uniform vec3 uSunDir;
uniform float uTLit;
uniform float uTMid;
uniform vec3 uBand[kSurfaces * 3];    // per surface: shadow, mid, lit
uniform vec3 uDetail[kSurfaces * 3];  // tufts, leaf speckles, water streaks
uniform mat4 uLightViewProj;
uniform sampler2DShadow uShadowMap;
uniform float uNormalBias;
uniform float uShadowTexel;  // one shadow-map texel in UV
uniform vec4 uHorizon;       // ridge centre xz, radius, mean height (radius 0: none)
uniform float uHorizonRelief;
uniform float uCloudCover;   // 0 clear, 1 overcast
uniform vec2 uCloudOffset;   // drift, in cloud-noise units
uniform float uCloudStrength;  // 0..1, how dark a thick cloud's shadow gets

vec3 bandColour(int surface, int band) {
    return uBand[surface * 3 + band];
}

// Tone runs 0 (shadow) to 2 (lit) in quarter steps; between two bands the
// colour moves through three in-between shades. A face turning away from
// the sun, or a shadow edge passing over a tuft, then steps through the
// ramp instead of snapping from one band to the next.
const float kToneSteps = 4.0;
const float kToneSoftness = 0.09;  // N·L width of each band transition

vec3 rampColour(vec3 shadow, vec3 mid, vec3 lit, float tone) {
    return tone < 1.0 ? mix(shadow, mid, tone) : mix(mid, lit, tone - 1.0);
}

vec3 toneColour(int surface, float tone) {
    int i = surface * 3;
    return rampColour(uBand[i], uBand[i + 1], uBand[i + 2], tone);
}

vec3 toneDetail(int surface, float tone) {
    int i = surface * 3;
    return rampColour(uDetail[i], uDetail[i + 1], uDetail[i + 2], tone);
}

// Fraction of the light that reaches this point: a 3x3 grid of filtered
// taps (the shadow sampler compares with linear filtering), about four
// texels across. Under a turning sun the stair-steps of a rasterised edge
// slide sideways past each point; a kernel this wide averages them out, so
// the fraction falls steadily instead of flickering on and off.
float sunVisibleSpread(vec3 wpos, vec3 n, float spread, float biasScale) {
    vec4 ls = uLightViewProj * vec4(wpos + n * uNormalBias * biasScale, 1.0);
    vec3 p = ls.xyz / ls.w * 0.5 + 0.5;
    float v = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            v += texture(uShadowMap, vec3(p.xy + vec2(x, y) * uShadowTexel * spread, p.z));
    return v / 9.0;
}

float sunVisible(vec3 wpos, vec3 n) { return sunVisibleSpread(wpos, n, 1.25, 1.0); }

// Hard, pixel-crisp shadow: in or out. Thresholding the filtered fraction
// rather than a single tap keeps the edge from crawling as the sun turns.
float sunLit(vec3 wpos, vec3 n, float biasScale) {
    return step(0.5, sunVisibleSpread(wpos, n, 1.25, biasScale));
}

// Ridge height around the ring, 0..1, by angle: a few broad hills, with
// integer frequencies so it is seamless. Kept smooth on purpose. Points a
// metre apart on one pillar face look out at slightly different parts of
// the ridge, so any fine tree-line detail would print as a wobble straight
// across the face; a smooth ridge gives a clean line the stonework breaks up.
float ridgeProfile(float a) {
    return clamp(0.5 + 0.30 * sin(2.0 * a + 0.7) + 0.20 * sin(5.0 * a + 2.1), 0.0, 1.0);
}

// 1 where light clears the ridge: trace from the point toward the light to
// the ring, look up the ridge height there, and compare with how high the
// light ray is by then. Analytic, so the edge glides with the sun and never
// shimmers, and hard, so it is a crisp pixel line like a cast shadow.
float horizonLight(vec3 wpos) {
    float radius = uHorizon.z;
    if (radius <= 0.0) return 1.0;
    vec3 L = normalize(uSunDir);
    float lh = length(L.xz);
    if (lh < 1e-4) return 1.0;
    vec2 dir = L.xz / lh;
    vec2 p = wpos.xz - uHorizon.xy;
    float b = dot(p, dir);
    float c = dot(p, p) - radius * radius;
    float t = -b + sqrt(max(b * b - c, 0.0));  // distance to the ring
    vec2 q = p + dir * t;
    float ridge = uHorizon.w + uHorizonRelief * (ridgeProfile(atan(q.y, q.x)) - 0.5);
    float clears = ridge - t * (L.y / lh);  // height at which this point sees the sun
    return step(clears, wpos.y);
}

float cloudHash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float cloudNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = cloudHash(i), b = cloudHash(i + vec2(1, 0));
    float c = cloudHash(i + vec2(0, 1)), d = cloudHash(i + vec2(1, 1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// How much cloud shadow falls here: 0 clear, up to uCloudStrength under a
// thick cloud. The layer is fractal noise drifting with the wind, read
// straight down so it never swings around with the sun. Density is cut into
// three steps, so a cloud's shadow has a stepped edge that thickens toward
// the middle instead of one hard, opaque blot.
float cloudShade(vec3 wpos) {
    if (uCloudCover <= 0.0 || uCloudStrength <= 0.0) return 0.0;
    vec2 p = wpos.xz / 26.0 + uCloudOffset;
    float n = 0.0, amp = 0.55;
    for (int i = 0; i < 4; ++i) {
        n += amp * cloudNoise(p);
        p = p * 2.03 + vec2(17.1, 9.7);
        amp *= 0.48;
    }
    // The fractal sum mostly lands in 0.3..0.7, so cover maps onto that range.
    float t = mix(0.70, 0.30, uCloudCover);
    float step = (n > t + 0.08) ? 1.0 : (n > t + 0.04) ? 0.66 : (n > t) ? 0.33 : 0.0;
    return step * uCloudStrength;
}

// A colour under cloud: pulled toward the surface's own shadow colour.
vec3 clouded(vec3 col, int surface, float shade) {
    return mix(col, bandColour(surface, 0), shade);
}

// N·L part of the tone, unquantised, 0 shadow .. 2 lit. Nothing is lit
// while the light is below the horizon.
float ndlTone(vec3 n) {
    if (uSunDir.y <= 0.0) return 0.0;
    float ndl = dot(normalize(n), uSunDir);
    return clamp((ndl - uTMid) / kToneSoftness + 0.5, 0.0, 1.0)
         + clamp((ndl - uTLit) / kToneSoftness + 0.5, 0.0, 1.0);
}

float quantiseTone(float t) { return floor(t * kToneSteps + 0.5) / kToneSteps; }

// Quantised tone, 0 shadow .. 2 lit, for curved surfaces: a band edge moves
// across them pixel by pixel as the sun turns, which already reads smooth.
// Shadows are hard, so a cast shadow is a crisp edge.
float toonTone(vec3 wpos, vec3 n) {
    float t = ndlTone(n);
    if (t > 0.0) t *= sunLit(wpos, n, 1.0) * horizonLight(wpos);
    return quantiseTone(t);
}

// Tone for flat-shaded faces (cut stone). A face has one normal, so a
// quantised N·L would make the whole face jump a step at a time as the sun
// or moon moves. Here N·L glides while shadow and ridge edges stay hard, so
// a face shows one colour per side, that colour eases through the ramp, and
// the shadow across it is a crisp pixel edge.
float toonToneFlat(vec3 wpos, vec3 n) {
    float t = ndlTone(n);
    // Chipped stone sits up to ~12 cm off its flat box face, so it needs a
    // bias bigger than its own bumps or they shadow the face in speckles that
    // crawl as the light moves.
    if (t > 0.0) t *= sunLit(wpos, n, 2.5) * horizonLight(wpos);
    return t;
}
