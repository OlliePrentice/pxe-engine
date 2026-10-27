// Shared toon ramp. Spliced into the mesh fragment shader and the grass
// vertex shader so both run the same function on the same kind of inputs.
const int kSurfaces = 11;  // pxe::kSurfCount
const int kSurfStone = 2;
const int kSurfBark = 3;
const int kSurfLeaf = 4;
const int kSurfWater = 5;
const int kSurfFoam = 6;
const int kSurfWood = 7;
const int kSurfPlaster = 8;
const int kSurfRoof = 9;
const int kSurfOak = 10;
// Each pixel's surface goes to the edge pass in the normal target's alpha,
// as (surface + 1) / kSurfaceScale; 0 marks a grass sprite and 1 a leaf
// sprite (edge.frag). Past the scene's surfaces, 11 marks an actor's
// creased material (mesh.frag's kSurfActorCrease).
const float kSurfaceScale = 16.0;

// Built structure - cut stone, and a building's wood, plaster, roof and
// oak - is flat-faced: one colour per side.
bool flatFaced(int surface) {
    return surface == kSurfStone || (surface >= kSurfWood && surface <= kSurfRoof) || surface == kSurfOak;
}

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
uniform float uOvercast;       // 0..1, rain: everything pulled toward shadow
uniform float uRain;           // 0 dry .. 1 downpour
uniform int uIndoors;          // 1 in a room (pxe::Scene::indoors): rain never lifts its shade
uniform float uFill;           // indoors: the room's fill light, 0 (night) .. 1 (day, golden hour)
uniform vec3 uFillDir;         // toward it (pxe::Scene::fillDir)
// Indoors: the sky's light through the room's windows (pxe::Scene::windows),
// how strong it is now, and the tint it and the fill light take.
const int kSkyWindows = 8;     // Renderer: windows past this are left out
uniform int uWindowCount;
uniform vec3 uWindowCentre[kSkyWindows];  // the middle of each opening, in the wall's inner face
uniform vec3 uWindowInward[kSkyWindows];  // level, into the room
uniform vec2 uWindowHalf[kSkyWindows];    // half its width and height
uniform float uSkyLight;       // 1 by day .. a faint moonlight at night
uniform vec3 uSkyTint;         // warm white, amber, violet-blue, cool moonlight
uniform float uFlash;          // lightning, 0 none .. 1 peak
uniform vec3 uFlashDir;        // toward the flash
uniform mat4 uFlashViewProj;
uniform sampler2DShadow uFlashShadowMap;
const int kFireLights = 16;     // Renderer::kFireLights
uniform int uFireCount;         // firelights this frame
uniform vec3 uFirePos[kFireLights];
uniform float uFireLight[kFireLights];     // strength, flickering with the flames
uniform int uFireCube[kFireLights];        // its cube in uFireShadow, -1 none
uniform float uFireShadowed[kFireLights];  // 0 no shadows .. 1 full, fading as cubes change hands
uniform float uFireReach;       // metres the light carries
uniform float uFireDim;         // how far direct sun or moon light drowns it out, 0..1
uniform vec3 uFireBand[kSurfaces];  // each surface's colour in full firelight
uniform samplerCubeArray uFireShadow;  // per cube, distance from its fire to the nearest surface, per direction

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
float visibleIn(sampler2DShadow map, mat4 viewProj, vec3 wpos, vec3 n, float spread, float biasScale) {
    vec4 ls = viewProj * vec4(wpos + n * uNormalBias * biasScale, 1.0);
    vec3 p = ls.xyz / ls.w * 0.5 + 0.5;
    float v = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            v += texture(map, vec3(p.xy + vec2(x, y) * uShadowTexel * spread, p.z));
    return v / 9.0;
}

float sunVisibleSpread(vec3 wpos, vec3 n, float spread, float biasScale) {
    return visibleIn(uShadowMap, uLightViewProj, wpos, n, spread, biasScale);
}

float sunVisible(vec3 wpos, vec3 n) { return sunVisibleSpread(wpos, n, 1.25, 1.0); }

// Hard, pixel-crisp shadow: in or out. Thresholding the filtered fraction
// rather than a single tap keeps the edge from crawling as the sun turns.
// Under rain cloud there is no direct light to cast them, so cast shadows
// fade as rain sets in: full in a drizzle, lighter as it gets heavier, and
// gone in a downpour, leaving only each face's own side shading. Indoors
// the shade is the room's own walls and roof, not the sun's shadow, so it
// stays; what fades instead is the direct sun falling in through the
// windows, toward the shade round it, gone in a downpour.
float sunLit(vec3 wpos, vec3 n, float biasScale) {
    float lit = step(0.5, sunVisibleSpread(wpos, n, 1.25, biasScale));
    float fade = smoothstep(0.15, 0.95, uRain);
    return uIndoors != 0 ? lit * (1.0 - fade) : mix(lit, 1.0, fade);
}

// A lightning flash has its own shadow map, and its shadows stay hard
// through any rain.
float flashLit(vec3 wpos, vec3 n, float biasScale) {
    return step(0.5, visibleIn(uFlashShadowMap, uFlashViewProj, wpos, n, 1.25, biasScale));
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
    // Fades under rain cloud like the cast shadows (see sunLit).
    return mix(step(clears, wpos.y), 1.0, smoothstep(0.15, 0.95, uRain));
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
// the middle instead of one hard, opaque blot. Rain adds an even overcast.
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
        float step = (n > t + 0.08) ? 1.0 : (n > t + 0.04) ? 0.66 : (n > t) ? 0.33 : 0.0;
        shade = step * uCloudStrength;
    }
    // Rain clouds over everything: an even overcast on top of the shadows.
    return 1.0 - (1.0 - shade) * (1.0 - uOvercast);
}

// Whether the fire sees this point, from its cube of distances: a hard
// shadow, so pillars, stones and logs throw crisp shadows away from it.
float fireVisible(vec3 wpos, vec3 n, vec3 firePos, int cube) {
    vec3 d = wpos + normalize(n) * 0.06 - firePos;
    return length(d) - 0.08 < texture(uFireShadow, vec4(d, float(cube))).r ? 1.0 : 0.0;
}

// Firelight, after the glow round t3ssel8r's fire: faces turned to the fire
// go a soft orange and faces turned away keep the night's colours, and the
// light fades smoothly with distance, through pinks into the night, rather
// than in rings. It casts shadows. A lit surface moves toward its own colour
// in full firelight. `sunlight` (0 shade .. 1 full sun or moon) is how much
// direct light the point already has: by day that drowns the fire out, so
// the glow shows in the shadows round it - of a tree, a pillar, a cloud -
// and not on the sunlit lawn.
vec3 fireLitBy(vec3 col, int surface, vec3 wpos, vec3 n, float sunlight, int i) {
    vec3 d = uFirePos[i] - wpos;
    float dist = length(d);
    if (dist > uFireReach) return col;
    float fall = 1.0 - dist / uFireReach;
    float ndl = clamp(dot(normalize(n), d / max(dist, 1e-3)) * 0.8 + 0.2, 0.0, 1.0);
    float k = pow(fall, 1.4) * ndl * uFireLight[i] * (1.0 - uFireDim * clamp(sunlight, 0.0, 1.0));
    if (k <= 0.003) return col;
    if (uFireCube[i] >= 0) k *= mix(1.0, fireVisible(wpos, n, uFirePos[i], uFireCube[i]), uFireShadowed[i]);
    return mix(col, max(col, uFireBand[surface]), clamp(k, 0.0, 0.85));
}

// Every firelight in turn. The renderer builds these shaders twice: with
// ONE_FIRE_LIGHT for frames with at most one firelight, and without for the
// rest. A loop here changes how the compiler rounds the first light's sums,
// enough to move a hard firelight shadow edge by a pixel, so the one-light
// build leaves it out and a single fire looks exactly as it always has.
vec3 fireLit(vec3 col, int surface, vec3 wpos, vec3 n, float sunlight) {
#ifdef ONE_FIRE_LIGHT
    if (uFireCount > 0) col = fireLitBy(col, surface, wpos, n, sunlight, 0);
#else
    for (int i = 0; i < uFireCount; ++i) col = fireLitBy(col, surface, wpos, n, sunlight, i);
#endif
    return col;
}

// A colour under cloud: pulled toward the surface's own shadow colour.
vec3 clouded(vec3 col, int surface, float shade) {
    return mix(col, bandColour(surface, 0), shade);
}

// N·L part of the tone, unquantised, 0 shadow .. 2 lit. Nothing is lit
// while the light is below the horizon.
float ndlToneFor(vec3 n, vec3 L) {
    if (L.y <= 0.0) return 0.0;
    float ndl = dot(normalize(n), L);
    return clamp((ndl - uTMid) / kToneSoftness + 0.5, 0.0, 1.0)
         + clamp((ndl - uTLit) / kToneSoftness + 0.5, 0.0, 1.0);
}

float ndlTone(vec3 n) { return ndlToneFor(n, uSunDir); }

// The light part of the tone, 0..2: N·L toward the sun or moon cut by their
// shadows and the ridge. A lightning flash is a second light blended over
// it by the flash's strength, so as the flash fades its shadows lighten
// band by band into the scene's own light rather than snapping off.
float litTone(vec3 wpos, vec3 n, float biasScale) {
    float t = ndlTone(n);
    if (t > 0.0) t *= sunLit(wpos, n, biasScale) * horizonLight(wpos);
    if (uFlash > 0.0) {
        float f = ndlToneFor(n, uFlashDir);
        if (f > 0.0) f *= flashLit(wpos, n, biasScale);
        t = mix(t, f, uFlash);
    }
    return t;
}

float quantiseTone(float t) { return floor(t * kToneSteps + 0.5) / kToneSteps; }

// Indoors, the sky lights the room through each window: a soft light that
// falls from the nearest point of the opening onto what is in front of it,
// by how squarely it is in front (none along the window's own wall), how
// far the face turns toward the opening, and fading with distance to
// nothing at kSkyReach. No shadows: it is the open sky's light, not a beam.
// Returns its share of the tone, 0 .. about 1 right in front of a window.
const float kSkyReach = 7.0;  // metres
const float kSkyGain = 2.6;
float skyLit(vec3 wpos, vec3 n) {
    if (uSkyLight <= 0.0) return 0.0;
    float k = 0.0;
    for (int i = 0; i < uWindowCount; ++i) {
        vec3 inward = uWindowInward[i];
        vec3 across = vec3(-inward.z, 0.0, inward.x);
        vec3 d = wpos - uWindowCentre[i];
        vec2 h = uWindowHalf[i];
        vec3 q = uWindowCentre[i] + across * clamp(dot(d, across), -h.x, h.x) + vec3(0.0, clamp(d.y, -h.y, h.y), 0.0);
        vec3 v = wpos - q;
        float dist = length(v);
        if (dist >= kSkyReach || dist < 1e-3) continue;
        vec3 l = v / dist;
        float ahead = dot(l, inward);
        if (ahead <= 0.0) continue;
        float fall = 1.0 - dist / kSkyReach;
        k += ahead * clamp(dot(n, -l), 0.0, 1.0) * fall * fall;
    }
    return uSkyLight * kSkyGain * k;
}

// Indoors, a fill light from a fixed direction lifts every face by how far
// it turns toward it, up to the mid tone, so the top, front and side of a
// table each take their own tone instead of one flat shadow tone; and the
// sky's light through the windows lifts what is in front of them. Direct
// light (sun or moon through a window, a flash) goes on over both: the
// tone is `lit` laid over that light as a screen, never past the lit tone,
// so a patch of daylight is always brighter than the room round it. What
// the fill and sky light add takes the sky's tint for the time of day
// (uSkyTint); the direct light keeps the palette's own colours. Flat faces
// keep the tone unquantised (toonToneFlat); the rest step through the
// ramp as toonTone does.
const float kMaxAmbient = 1.5;  // the fill and sky light together, in tone
vec3 indoorColour(int surface, float lit, vec3 wpos, vec3 n, bool flatFace) {
    vec3 nn = normalize(n);
    float fill = uFill * clamp(dot(nn, uFillDir), 0.0, 1.0);
    float ambient = min(fill + skyLit(wpos, nn), kMaxAmbient);
    float t = ambient + lit * (1.0 - 0.5 * ambient);
    if (!flatFace) t = quantiseTone(t);
    vec3 direct = toneColour(surface, lit);
    return direct + (toneColour(surface, t) - direct) * uSkyTint;
}

float barkHash(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

float barkNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = barkHash(i), n100 = barkHash(i + vec3(1, 0, 0));
    float n010 = barkHash(i + vec3(0, 1, 0)), n110 = barkHash(i + vec3(1, 1, 0));
    float n001 = barkHash(i + vec3(0, 0, 1)), n101 = barkHash(i + vec3(1, 0, 1));
    float n011 = barkHash(i + vec3(0, 1, 1)), n111 = barkHash(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

// Bark, painted rather than carved: the smooth light is pushed around by
// noise stretched up the trunk and cut into half-band steps, so the bark
// breaks into soft, irregular blotches of a few close shades, like the
// watercolour trunks in the video. The shadow side keeps faint blotches.
float barkTone(vec3 wpos, vec3 n) {
    float lit = litTone(wpos, n, 1.0);
    vec3 q = wpos * vec3(2.4, 0.55, 2.4);
    // Broad blotches, smaller ones, and fine streaks drawn up the trunk.
    float w = 0.5 * barkNoise(q) + 0.25 * barkNoise(q * 2.3 + 7.1)
            + 0.25 * barkNoise(wpos * vec3(7.0, 0.35, 7.0) + 3.3);
    float t = clamp(lit + (w - 0.5) * 1.4, 0.0, 2.0);
    return floor(t * 2.0 + 0.5) * 0.5;
}

// Quantised tone, 0 shadow .. 2 lit, for curved surfaces: a band edge moves
// across them pixel by pixel as the sun turns, which already reads smooth.
// Shadows are hard, so a cast shadow is a crisp edge.
float toonTone(vec3 wpos, vec3 n) {
    return quantiseTone(litTone(wpos, n, 1.0));
}

// Tone for flat-shaded faces (cut stone). A face has one normal, so a
// quantised N·L would make the whole face jump a step at a time as the sun
// or moon moves. Here N·L glides while shadow and ridge edges stay hard, so
// a face shows one colour per side, that colour eases through the ramp, and
// the shadow across it is a crisp pixel edge.
float toonToneFlat(vec3 wpos, vec3 n) {
    // Chipped stone sits up to ~12 cm off its flat box face, so it needs a
    // bias bigger than its own bumps or they shadow the face in speckles that
    // crawl as the light moves.
    return litTone(wpos, n, 2.5);
}

#ifdef FIRE_GRID
// Grass fire (see FireGrid): how burnt the ground is under a point, 0
// untouched .. 1 burnt out, from the burn texture the burning cells paint.
// Only the programs built with FIRE_GRID read it, so the scene without a
// grass fire is drawn by exactly the shaders it always was.
uniform sampler2D uBurn;
uniform vec4 uBurnDomain;  // xz of its corner, 1 / its size in metres

float burnAt(vec3 wpos) {
    vec2 uv = (wpos.xz - uBurnDomain.xy) * uBurnDomain.zw;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 0.0;
    return textureLod(uBurn, uv, 0.0).r;
}

// How much of the crown over a point has burnt away, 0 none .. 1 all.
float crownBurnAt(vec3 wpos) {
    vec2 uv = (wpos.xz - uBurnDomain.xy) * uBurnDomain.zw;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 0.0;
    return textureLod(uBurn, uv, 0.0).g;
}

// A crown burns away in holes a third of a metre across, scattered so the
// last of it goes in ragged patches (shadow.frag makes the same holes).
bool crownGone(float burnt, vec3 wpos) {
    vec3 q = floor(wpos * 3.0);
    return burnt > 0.5 + 0.45 * fract(sin(dot(q, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

// Burnt grass and the ground under it, at a tone: scorched (1) to a dry
// brown between the grass and the bare dirt, then charred (2) near black.
vec3 burntColour(float tone, int level) {
    vec3 dirt = toneColour(1, tone);
    if (level >= 2) return dirt * vec3(0.42, 0.4, 0.42);
    return mix(toneColour(0, tone), dirt, 0.65) * vec3(1.04, 0.94, 0.74);
}

// Firelight on burnt grass: charred (1) it takes half the light the lawn
// would, so the burn still reads as dark in the light of a fire.
vec3 charLit(vec3 col, vec3 lit, float charred) {
    return mix(lit, col + (lit - col) * 0.5, charred);
}
#endif
