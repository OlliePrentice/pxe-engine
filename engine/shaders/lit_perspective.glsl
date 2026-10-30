// The perspective mode's smooth lit model (pxe::FirstPersonCamera), spliced
// into mesh_perspective.frag in place of common.glsl's toon ramp. A fork of
// the parts of common.glsl the lit model needs, copied rather than shared so
// that tuning either mode's light never changes the other's.
//
// Each surface has one base colour, its albedo. Linear light:
//   albedo * (sun * N.L * shadow * (1 - cloud) + skyFill * up + bounce * down)
// plus a lightning flash, times the exposure, clamped, and written as sRGB.
// Shadows are soft (a fixed 16-tap pattern over a penumbra of fixed width in
// metres) and cloud shadow is continuous.
const int kSurfaces = 12;  // pxe::kSurfCount
const int kSurfStone = 2;
const int kSurfWood = 7;
const int kSurfRoof = 9;
const int kSurfOak = 10;
// Each pixel's surface goes to the edge pass in the normal target's alpha,
// as (surface + 1) / kSurfaceScale (edge_perspective.frag).
const float kSurfaceScale = 16.0;

// Built structure - cut stone, and a building's wood, plaster, roof and
// oak - is flat-faced: one colour per side.
bool flatFaced(int surface) {
    return surface == kSurfStone || (surface >= kSurfWood && surface <= kSurfRoof) || surface == kSurfOak;
}

uniform vec3 uSunDir;           // toward the light (lighting.lightDir, normalised)
uniform vec3 uSunLight;         // linear
uniform vec3 uSkyFillLight;     // linear
uniform vec3 uBounceLight;      // linear
uniform float uExposure;
uniform vec3 uAlbedo[kSurfaces];        // linear
uniform mat4 uLightViewProj;
uniform sampler2DShadow uShadowMap;
uniform float uNormalBias;
uniform float uPenumbraUV;      // half the penumbra, in shadow-map UV: shadowPenumbra / 2 / (2 * kShadowReach)
uniform float uPenumbraDepth;   // half the penumbra, metres, in the map's depth units: shadowPenumbra / 2 / depthSpan
uniform vec4 uHorizon;          // ridge centre xz, radius, mean height (radius 0: none)
uniform float uHorizonRelief;
uniform float uCloudCover;      // 0 clear, 1 overcast
uniform vec2 uCloudOffset;      // drift, in cloud-noise units
uniform float uCloudStrength;   // 0..1, how dark a thick cloud's shadow gets
uniform float uOvercast;        // 0..1, rain: everything pulled toward shadow
uniform float uRain;            // 0 dry .. 1 downpour
uniform float uFlash;           // lightning, 0 none .. 1 peak
uniform vec3 uFlashDir;         // toward the flash
uniform mat4 uFlashViewProj;
uniform sampler2DShadow uFlashShadowMap;
uniform float uTerminator[kSurfaces];  // per surface, 0 smooth .. 1 sharpest light-to-shadow edge

const vec2 kPoisson[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725), vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464), vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420), vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590), vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

// Fraction of the light reaching wpos: 16 hardware-filtered taps over a
// disc the penumbra wide, in a fixed pattern (no per-pixel rotation, which
// crawls on the low-res target). A receiver tilted to the light steps in
// depth across the disc, so each tap's compare depth is pulled toward the
// light by the disc's radius times the tilt's tangent (receiver-plane
// bias), the tangent held to 4.
float softVisible(sampler2DShadow map, mat4 viewProj, vec3 wpos, vec3 n, vec3 L, float biasScale) {
    vec4 ls = viewProj * vec4(wpos + n * uNormalBias * biasScale, 1.0);
    vec3 p = ls.xyz / ls.w * 0.5 + 0.5;
    float c = clamp(dot(n, L), 0.05, 1.0);
    float slope = min(sqrt(1.0 - c * c) / c, 4.0);
    float z = p.z - uPenumbraDepth * slope;
    float v = 0.0;
    for (int i = 0; i < 16; ++i) v += texture(map, vec3(p.xy + kPoisson[i] * uPenumbraUV, z));
    return v / 16.0;
}

// The sun's shadow, fading under rain cloud: with no direct light to cast
// them, cast shadows are full in a drizzle, lighter as rain gets heavier and
// gone in a downpour.
float sunVisible(vec3 wpos, vec3 n, float biasScale) {
    float v = softVisible(uShadowMap, uLightViewProj, wpos, n, uSunDir, biasScale);
    return mix(v, 1.0, smoothstep(0.15, 0.95, uRain));
}

// A lightning flash has its own shadow map, and its shadows stay through any rain.
float flashVisible(vec3 wpos, vec3 n, float biasScale) {
    return softVisible(uFlashShadowMap, uFlashViewProj, wpos, n, uFlashDir, biasScale);
}

// Ridge height around the ring, 0..1, by angle: a few broad hills, with
// integer frequencies so it is seamless. Kept smooth on purpose.
float ridgeProfile(float a) {
    return clamp(0.5 + 0.30 * sin(2.0 * a + 0.7) + 0.20 * sin(5.0 * a + 2.1), 0.0, 1.0);
}

// 1 where light clears the ridge: trace from the point toward the light to
// the ring, look up the ridge height there, and compare with how high the
// light ray is by then. Analytic, and soft over a metre of height.
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
    // Fades under rain cloud like the cast shadows (see sunVisible).
    return mix(smoothstep(clears - 0.5, clears + 0.5, wpos.y), 1.0, smoothstep(0.15, 0.95, uRain));
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
// straight down so it never swings around with the sun. Its density goes
// continuously from clear to full over a narrow band, so a cloud's shadow has
// a soft edge. Rain adds an even overcast on top.
float cloudShade(vec3 wpos) {
    float shade = 0.0;
    if (uCloudCover > 0.0 && uCloudStrength > 0.0) {
        vec2 p = wpos.xz / 26.0 + uCloudOffset;
        float n = 0.0, amp = 0.55;
        for (int i = 0; i < 4; ++i) {
            n += amp * cloudNoise(p);
            p = p * 2.03 + vec2(17.1, 9.7);
            amp *= 0.48;
        }
        // The fractal sum mostly lands in 0.3..0.7, so cover maps onto that range.
        float t = mix(0.70, 0.30, uCloudCover);
        shade = smoothstep(t, t + 0.08, n) * uCloudStrength;
    }
    return 1.0 - (1.0 - shade) * (1.0 - uOvercast);
}

const vec3 kFlashLight = vec3(1.6, 1.75, 2.0);  // a lightning flash at its peak, cold blue-white
// terminator: 0 is the plain falloff; above 0 the direct term is
// pow(N.L, 1 - 0.85 * terminator), steep at the terminator and graded across
// lit faces.
vec3 litColour(vec3 albedo, vec3 wpos, vec3 n, float biasScale, float terminator) {
    float direct = 0.0;
    if (uSunDir.y > 0.0) {
        float ndl = max(dot(n, uSunDir), 0.0);
        if (terminator > 0.0 && ndl > 0.0) ndl = pow(ndl, 1.0 - 0.85 * terminator);
        if (ndl > 0.0) direct = ndl * sunVisible(wpos, n, biasScale) * horizonLight(wpos);
    }
    float shade = cloudShade(wpos);
    vec3 light = uSunLight * direct * (1.0 - shade)
               + uSkyFillLight * (0.5 + 0.5 * n.y)
               + uBounceLight * (0.5 - 0.5 * n.y);
    if (uFlash > 0.0) {
        float f = max(dot(n, uFlashDir), 0.0);
        if (f > 0.0) light += kFlashLight * uFlash * f * flashVisible(wpos, n, biasScale);
    }
    return albedo * light * uExposure;
}
vec3 toSrgb(vec3 c) {  // the exact sRGB curve, per channel, after clamping to 0..1
    c = clamp(c, 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}
