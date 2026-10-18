#version 410 core

// Post pass over the low-res target.
//
// Edges, after David Holland's write-up of t3ssel8r's look: only stone and
// bark take them. A pixel on an upward face whose neighbour turns away (a
// crease) or drops behind (a silhouette) catches a light rim; any other
// silhouette pixel darkens into an outline.
//
// Rain, after t3ssel8r's rain shader: streaks, splashes and a cool, murky
// grade, all drawn here from hashes. Splashes are hashed on cells of the
// surfaces themselves, rebuilt from depth, so they stay put on the world as
// the camera pans or turns. Streaks sit on the pixel grid in two layers at
// different depths, each moved across the screen as the camera turns as if
// it hung that far behind or in front of the focus.

uniform sampler2D uColor;
uniform sampler2D uNormal;  // xyz normal * 0.5 + 0.5, a = (surface + 1) / 8; 0 grass sprites, 1 leaf sprites
uniform sampler2D uDepth;
uniform float uDepthMetres;  // far - near: the ortho depth buffer is linear
uniform int uEnabled;

const int kSurfStone = 2;
const int kSurfBark = 3;
const int kSurfLeaf = 4;
const int kSurfWater = 5;
const int kLeafSprite = 7;
const float kSilhouette = 0.35;  // metres of depth step that count as an outline
uniform vec3 uLight;  // toward the sun or moon
uniform vec3 uRim;  // blended with the time of day
uniform vec3 uInk;

uniform float uRain;        // 0 dry .. 1 downpour
uniform float uFlash;       // lightning, 0 none .. 1 peak
uniform float uTime;        // seconds
uniform vec2 uCamTexel;     // camera position in whole low-res texels
uniform float uRainSlant;   // pixels sideways per pixel of fall; positive leans right
uniform vec3 uRainColour;   // streaks, splashes and haze, blended with the time of day
uniform vec3 uEye;          // camera, for rebuilding world positions from depth
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec3 uFwd;
uniform vec2 uOrthoHalf;    // half the view in metres
uniform float uNear;
uniform float uYawTexels;   // camera yaw in radians over the texel size in metres
uniform sampler2DShadow uShadowMap;  // the sun's, for god rays
uniform mat4 uLightViewProj;
uniform float uRays;        // 0 none .. about 1 strong
uniform vec3 uRayColour;
uniform vec2 uRaySpan;      // heights of the lowest and highest slices of air, metres
uniform sampler2D uCanopy;  // tiling noise for the gaps light falls through
uniform float uCloudCover;  // cloud shadows, as in the toon shading: they block the rays
uniform vec2 uCloudOffset;
uniform float uCloudStrength;
uniform sampler2D uHeat;    // fire: flame heat (r), ember glow (g), smoke (b)
uniform int uFire;

out vec4 oColor;

vec3 edges(ivec2 p, vec3 col, vec4 np, int surface) {
    if (uEnabled == 0 || (surface != kSurfStone && surface != kSurfBark)) return col;
    vec3 n = normalize(np.xyz * 2.0 - 1.0);
    float d = texelFetch(uDepth, p, 0).r * uDepthMetres;

    bool rim = false;
    bool ink = false;
    bool stroke = false;
    // Diagonal neighbours count for rims, so a seam that rasterises as a
    // 2:1 staircase joins into one line instead of a row of dots.
    const ivec2 kOffsets[8] = ivec2[8](ivec2(0, 1), ivec2(0, -1), ivec2(1, 0), ivec2(-1, 0),
                                      ivec2(1, 1), ivec2(-1, 1), ivec2(1, -1), ivec2(-1, -1));
    for (int i = 0; i < 8; ++i) {
        bool diagonal = i >= 4;
        ivec2 q = p + kOffsets[i];
        float dq = texelFetch(uDepth, q, 0).r * uDepthMetres;
        vec4 nq4 = texelFetch(uNormal, q, 0);
        vec3 nq = normalize(nq4.xyz * 2.0 - 1.0);
        bool behind = dq - d > kSilhouette;
        bool meshNeighbour = nq4.a > 0.0 && nq4.a < 0.99;  // not a grass or leaf sprite
        bool crease = !behind && abs(dq - d) < kSilhouette && meshNeighbour && dot(n, nq) < 0.8;
        if (n.y > 0.6 && (behind || (crease && nq.y < n.y - 0.3))) rim = true;
        else if (behind && !diagonal) ink = true;
        // Bark is smooth-shaded except along a few seams marked sharp in
        // Blender; there the normal jumps a little, and one light stroke is
        // drawn on the side that faces the light more.
        if (surface == kSurfBark && !behind && !diagonal && abs(dq - d) < kSilhouette && meshNeighbour &&
            dot(n, nq) < 0.97 && dot(n, uLight) > dot(nq, uLight)) stroke = true;
    }
    // Bark is painted, not carved: no rims, a faint outline, and its few
    // seam strokes are soft.
    if (surface == kSurfBark) rim = false;
    if (stroke) return mix(col, uRim, 0.15);
    if (rim) return mix(col, uRim, 0.4);
    if (ink) return mix(col, uInk, surface == kSurfBark ? 0.15 : 0.35);
    return col;
}

vec3 hash3(vec2 c) {
    vec3 p = fract(vec3(c.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yzz) * p.zyx);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash3(i).x, b = hash3(i + vec2(1, 0)).x, c = hash3(i + vec2(0, 1)).x, d = hash3(i + vec2(1, 1)).x;
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// One layer of streaks, as in the video: streaks do not slide down the
// screen; each holds still for a moment and then the layer is redrawn
// somewhere new, `rate` times a second, which gives the rain its stepped,
// low-frame-rate flicker. The pixel grid is cut into cells, and a cell holds
// a one-pixel streak, leaning with the wind, with probability `density`.
// Returns 1 on a streak pixel.
// The world position under pixel p, and the pixel a world position lands on.
vec3 worldAt(ivec2 p) {
    vec2 s = ((vec2(p) + 0.5) / vec2(textureSize(uDepth, 0)) * 2.0 - 1.0) * uOrthoHalf;
    float d = uNear + texelFetch(uDepth, p, 0).r * uDepthMetres;
    return uEye + uRight * s.x + uUp * s.y + uFwd * d;
}

vec2 pixelOf(vec3 wpos) {
    vec2 s = vec2(dot(wpos - uEye, uRight), dot(wpos - uEye, uUp));
    return floor((s / uOrthoHalf * 0.5 + 0.5) * vec2(textureSize(uDepth, 0)));
}

// Splashes on a surface, which is cut into cells a quarter metre square in
// its own plane (ground-plane cells on tops and sprites, wall-plane cells on
// sides). A cell holds a splash with probability `chance`, which plays out
// on its own beat at a point fixed in the cell; the splash's shape is drawn
// in screen pixels around where that point lands. `big` gives the
// three-stage splash (a dot, a little crown, two droplets), otherwise a dot
// then a small pop. Returns true on a splash pixel.
bool splash(ivec2 p, vec3 wpos, vec3 n, bool top, float chance, float seed, bool big) {
    const float kCell = 0.25;
    vec3 a = top ? vec3(1.0, 0.0, 0.0) : normalize(vec3(-n.z, 0.0, n.x) + 1e-5);
    vec3 b = top ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec2 uv = vec2(dot(wpos, a), dot(wpos, b));
    vec2 base = floor(uv / kCell);
    bool hit = false;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            vec2 id = base + vec2(x, y);
            vec3 h = hash3(id + seed);
            if (h.x >= chance) continue;
            vec2 c = (id + 0.2 + 0.6 * h.yz) * kCell;
            vec2 dd = vec2(p) - pixelOf(wpos + a * (c.x - uv.x) + b * (c.y - uv.y));
            float phase = fract(uTime * (1.4 + h.y) + h.z * 7.0);
            if (big) {
                if (phase < 0.05) hit = hit || dd == vec2(0.0) || dd == vec2(0.0, 1.0);
                else if (phase < 0.11) hit = hit || dd == vec2(0.0) || abs(dd) == vec2(1.0, 1.0) && dd.y > 0.0;
                else if (phase < 0.17) hit = hit || abs(dd) == vec2(2.0, 2.0) && dd.y > 0.0;
            } else {
                if (phase < 0.06) hit = hit || dd == vec2(0.0);
                else if (phase < 0.13) hit = hit || dd == vec2(0.0) || abs(dd) == vec2(1.0, 1.0) && dd.y > 0.0;
            }
        }
    return hit;
}

float streaks(vec2 w, float density, float rate, vec2 cell, float len, float seed) {
    float tick = floor(uTime * rate + seed * 0.37);
    vec2 q = vec2(floor(w.x + uRainSlant * w.y), w.y);
    vec2 id = floor(q / cell);
    vec3 h = hash3(id + seed + tick * vec2(17.3, 41.7));
    if (h.x > density) return 0.0;
    vec2 local = q - id * cell;
    float x0 = floor(h.y * (cell.x - 1.0));
    float y0 = floor(h.z * (cell.y - len));
    return (floor(local.x) == x0 && local.y >= y0 && local.y < y0 + len) ? 1.0 : 0.0;
}

// God rays, after t3ssel8r's: the air in front of each pixel is cut into
// level slices from the ground to the treetops, and each slice is looked up
// in the sun's shadow map - no ray marching through a volume, just one
// texture tap per slice. Light reaches a slice only through gaps in a
// canopy above the frame, a pattern fixed in the light's own space, so it
// falls in separate shafts along the sun; the scene's own shadows - crowns,
// trunks, pillars - cut the shafts further. The share of lit air, cut into
// a few hard steps, lays a warm haze over the pixel. Hard edges keep it
// crisp, like the rest of the pixel art.
float canopyGap(vec2 lightUv) {
    vec2 q = lightUv * 0.8 + vec2(uTime * 0.0005, 0.0);
    return texture(uCanopy, q).r > 0.6 ? 1.0 : 0.0;
}

// The cloud shadow over a point, as the toon shading draws it: 0 clear, up
// to the shadow strength under a thick cloud.
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

float cloudShadow(vec3 wpos) {
    if (uCloudCover <= 0.0 || uCloudStrength <= 0.0) return 0.0;
    vec2 q = wpos.xz / 26.0 + uCloudOffset;
    float n = 0.0, amp = 0.55;
    for (int i = 0; i < 4; ++i) {
        n += amp * cloudNoise(q);
        q = q * 2.03 + vec2(17.1, 9.7);
        amp *= 0.48;
    }
    float t = mix(0.70, 0.30, uCloudCover);
    return ((n > t + 0.08) ? 1.0 : (n > t + 0.04) ? 0.66 : (n > t) ? 0.33 : 0.0) * uCloudStrength;
}

vec3 godRays(ivec2 p, vec3 col) {
    if (uRays <= 0.0) return col;
    if (texelFetch(uDepth, p, 0).r >= 0.9999) return col;
    vec3 s = worldAt(p);
    // Each pixel's slices sit a little higher or lower, by an interleaved
    // pattern on the world-anchored pixel grid, so the steps between slices
    // do not line up across the screen into streaks.
    vec2 w = vec2(p) + uCamTexel;
    float jitter = fract(52.9829189 * fract(dot(w, vec2(0.06711056, 0.00583715))));
    // A cloud over the column, at its foot or its top, blocks the beam.
    vec3 topOfColumn = s - uFwd * ((uRaySpan.y - s.y) / -uFwd.y);
    float cloud = max(cloudShadow(s), cloudShadow(topOfColumn));
    float strength = uRays * (1.0 - cloud * 1.4);
    // Skip the slices where even all the air lit would not reach the
    // faintest step: a high sun, under cloud, or the top of a pillar.
    float air = clamp((uRaySpan.y - s.y) / (uRaySpan.y - uRaySpan.x), 0.0, 1.0);
    if (air * strength <= 0.08) return col;
    const int kSlices = 32;
    float lit = 0.0;
    for (int i = 0; i < kSlices; ++i) {
        float h = mix(uRaySpan.x, uRaySpan.y, (float(i) + jitter) / float(kSlices));
        if (h <= s.y) continue;
        vec3 q = s - uFwd * ((h - s.y) / -uFwd.y);
        vec4 ls = uLightViewProj * vec4(q, 1.0);
        vec3 l = ls.xyz / ls.w * 0.5 + 0.5;
        if (canopyGap(l.xy) > 0.0) lit += texture(uShadowMap, vec3(l.xy, l.z - 0.002));
    }
    float v = lit / float(kSlices) * strength;
    float a = v > 0.45 ? 0.27 : (v > 0.3 ? 0.2 : (v > 0.18 ? 0.13 : (v > 0.08 ? 0.07 : 0.0)));
    return mix(col, uRayColour, a);
}

vec3 rain(ivec2 p, vec3 col, vec4 np, int surface) {
    if (uRain <= 0.0) return col;
    vec2 w = vec2(p) + uCamTexel;  // pixel grid anchored to the focus

    // Murky grade: cooler, flatter, darker, with an even haze over it all.
    // A lightning flash cuts through most of it while it lasts.
    float murk = uRain * (1.0 - 0.7 * uFlash);
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, l * vec3(0.86, 0.93, 1.06), 0.45 * murk) * (1.0 - 0.14 * murk);
    col = mix(col, uRainColour * 0.85, 0.1 * murk);

    // Wet stone and trunks pick up a soft light line along their upper
    // outlines, a two-pixel haze where they meet what is behind them.
    vec3 n = normalize(np.xyz * 2.0 - 1.0);
    bool hard = surface == kSurfStone || surface == kSurfBark;
    bool crown = surface == kSurfLeaf || surface == kLeafSprite;
    if (hard) {
        float d = texelFetch(uDepth, p, 0).r * uDepthMetres;
        float d1 = texelFetch(uDepth, p + ivec2(0, 1), 0).r * uDepthMetres;
        float d2 = texelFetch(uDepth, p + ivec2(0, 2), 0).r * uDepthMetres;
        float haze = (d1 - d > kSilhouette) ? 0.45 : ((d2 - d > kSilhouette) ? 0.2 : 0.0);
        col = mix(col, uRainColour, haze * uRain);
    }

    // Splashes: water gets ripples instead; stone and bark an occasional
    // three-stage splash; crowns and the lawn a light scatter of small ones.
    if (surface != kSurfWater) {
        vec3 wpos = worldAt(p);
        bool top = !hard || n.y > 0.6;
        float chance = hard ? (top ? 0.35 : 0.12) : (crown ? 0.25 : 0.12);
        if (splash(p, wpos, n, top, uRain * chance, hard ? 29.0 : 17.0, hard))
            col = mix(col, uRainColour, hard ? 0.75 : 0.65);
    }

    // Streaks in two sparse layers, the nearer longer and brighter, each
    // redrawn on its own beat so they never all jump at once. Kept thin so
    // the splashes and ripples still read.
    // The nearer layer hangs 6 m in front of the focus and the farther one
    // 10 m behind it, so as the camera turns they drift opposite ways.
    float s = streaks(w - vec2(6.0 * uYawTexels, 0.0), 0.14 * uRain, 11.0, vec2(11.0, 100.0), 30.0, 1.0);
    s = max(s, 0.7 * streaks(w + vec2(10.0 * uYawTexels, 0.0), 0.1 * uRain, 9.0, vec2(9.0, 80.0), 22.0, 2.0));
    col = mix(col, uRainColour, 0.55 * s);
    return col;
}

// Fire, after t3ssel8r's: the summed heat of the particles cut into three
// flat colours - a pale yellow core inside orange inside a crimson rim -
// and embers as single red pixels. It glows, so it goes on last, over the
// rain's grade. Smoke goes under it: a very faint haze in two steps that
// greys and lightens whatever is behind it.
vec3 fire(ivec2 p, vec3 col) {
    vec3 h = texelFetch(uHeat, p, 0).rgb;
    float smoke = h.b > 0.45 ? 0.2 : (h.b > 0.15 ? 0.11 : 0.0);
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(l * 1.2 + 0.05), smoke);
    if (h.g > 0.0) col = h.g > 0.5 ? vec3(0.94, 0.42, 0.24) : vec3(0.72, 0.18, 0.2);
    if (h.r > 0.22) col = h.r > 1.45 ? vec3(0.98, 0.89, 0.55) : (h.r > 0.5 ? vec3(0.95, 0.55, 0.3) : vec3(0.78, 0.2, 0.22));
    return col;
}

void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec3 col = texelFetch(uColor, p, 0).rgb;
    vec4 np = texelFetch(uNormal, p, 0);
    int surface = int(np.a * 8.0 + 0.5) - 1;
    col = edges(p, col, np, surface);
    col = godRays(p, col);
    col = rain(p, col, np, surface);
    if (uFire == 1) col = fire(p, col);
    oColor = vec4(col, 1.0);
}
