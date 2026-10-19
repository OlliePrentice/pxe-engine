#version 410 core

// Grass fire flames, straight from the fire grid into the heat buffer as
// fire.vert's particles are (and cut into flame colours the same way):
// per burning cell a low glow, two short flames that rise, shrink and cool, a puff of
// smoke now and then, and an ember - a spark flying up while it burns, a
// red pixel glowing on the burnt ground after. Like the campfire, each part
// steps at its own low frame rate. One instance per patch of cells. A
// crown burning over the cell adds a glow and a tall flame up at the top of
// its leaves, and smoke or a spark.

layout(location = 0) in vec2 aPatch;  // the patch's first cell

uniform sampler2D uState;  // the latest step
uniform sampler2D uPrev;   // the step before
uniform sampler2D uGround;
uniform sampler2D uGlow;      // the grid blurred (gridglow.frag), latest step
uniform sampler2D uGlowPrev;  // and the step before
uniform sampler2D uCanopy;        // crowns, latest step: r fire, a embers
uniform sampler2D uCanopyPrev;    // and the step before
uniform sampler2D uCanopyGround;  // g mean crown height, b crown top
uniform float uBlend;      // 0 the step before .. 1 the latest
uniform vec2 uMin;
uniform float uCell;
uniform int uPatch;
uniform mat4 uViewProj;
uniform vec2 uTarget;
uniform float uPxPerMetre;
uniform float uDepthBiasNdc;
uniform float uTime;
uniform vec2 uWind;  // metres per second

flat out float vHeat;
flat out float vKind;

const int kParts = 8;
// Crown flames sit among the leaves, so they are pulled nearer the camera
// to show through the crown's front: this many times the fire's own bias
// (a quarter metre), so 1.2 m.
const float kCrownBias = 4.8;

float hash(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

void drop() {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    gl_PointSize = 1.0;
    vHeat = 0.0;
    vKind = 0.0;
}

void emit(vec3 pos, float radius, float heat, float kind, float bias) {
    vec4 c = uViewProj * vec4(pos, 1.0);
    bool ember = kind > 1.5 && kind < 2.5;
    float size = ember ? 1.0 : max(1.0, floor(2.0 * radius * uPxPerMetre + 0.5));
    vec2 px = (c.xy / c.w * 0.5 + 0.5) * uTarget;
    px = mod(size, 2.0) > 0.5 ? floor(px) + 0.5 : floor(px + 0.5);
    gl_Position = vec4(px / uTarget * 2.0 - 1.0, c.z / c.w - uDepthBiasNdc * bias, 1.0);
    gl_PointSize = size;
    vHeat = heat;
    vKind = kind;
}

void main() {
    int part = gl_VertexID % kParts;
    int index = gl_VertexID / kParts;
    ivec2 cell = ivec2(aPatch) + ivec2(index % uPatch, index / uPatch);
    if (any(greaterThanEqual(cell, textureSize(uState, 0)))) {
        drop();
        return;
    }
    vec4 s = mix(texelFetch(uPrev, cell, 0), texelFetch(uState, cell, 0), uBlend);
    float fire = s.r, ember = s.a;
    vec3 base = vec3(uMin.x + (float(cell.x) + 0.5) * uCell, texelFetch(uGround, cell, 0).b,
                     uMin.y + (float(cell.y) + 0.5) * uCell);
    vec3 seed = vec3(vec2(cell), float(part) * 17.0);
    float h0 = hash(seed), h1 = hash(seed + 3.1), h2 = hash(seed + 7.7), h3 = hash(seed + 11.3);
    vec3 jitter = vec3(h2 - 0.5, 0.0, h3 - 0.5) * uCell;
    vec3 wind = vec3(uWind.x, 0.0, uWind.y);

    if (part >= 5) {
        // A burning crown, up toward the top of its leaves (they hide
        // anything lower): a glow at 8 fps, a tall flame at 12 fps, and from
        // some cells smoke at 6 fps or a spark at 10 fps. A crown just
        // catching burns in the middle of its leaves and climbs to the top
        // as it takes hold.
        float crown = mix(texelFetch(uCanopyPrev, cell, 0).r, texelFetch(uCanopy, cell, 0).r, uBlend);
        if (crown < 0.03) {
            drop();
            return;
        }
        vec4 kg = texelFetch(uCanopyGround, cell, 0);
        vec3 top = vec3(base.x, mix(kg.g, kg.b, 0.6 * smoothstep(0.1, 0.6, crown)), base.z) + jitter * 2.0;
        if (part == 5) {
            float t = floor(uTime * 8.0) / 8.0;
            float swell = 0.85 + 0.15 * sin(t * 8.0 + h0 * 6.3);
            emit(top, (0.16 + 0.06 * crown) * swell, 0.25 + 0.6 * crown, 0.0, kCrownBias);
        } else if (part == 6) {
            float t = floor(uTime * 12.0) / 12.0;
            float age = fract(t / (0.55 + 0.35 * h1) + h0);
            vec3 pos = top + vec3(0.0, 0.1 + age * 1.1 * crown, 0.0) + wind * 0.25 * age;
            emit(pos, mix(0.24, 0.06, age) * (0.6 + 0.4 * crown), crown * mix(1.6, 0.4, age), 1.0, kCrownBias);
        } else if (h1 < 0.3) {
            float t = floor(uTime * 6.0) / 6.0;
            float age = fract(t / 3.0 + h0);
            vec3 pos = top + vec3(0.0, 0.8 + age * 2.8, 0.0) + wind * 0.8 * age;
            emit(pos, mix(0.15, 0.38, age), 0.55 * crown * (1.0 - age), 3.0, kCrownBias);
        } else if (h1 < 0.45) {
            float t = floor(uTime * 10.0) / 10.0;
            float age = fract(t / 1.1 + h0);
            vec3 pos = top + vec3(0.0, 0.4 + age * 1.6, 0.0) + wind * 0.5 * age
                     + vec3(sin(age * 9.0 + h2 * 6.3), 0.0, cos(age * 7.0 + h3 * 6.3)) * 0.1;
            emit(pos, 0.0, 0.9 * (1.0 - age), 2.0, kCrownBias);
        } else {
            drop();
        }
        return;
    }
    if (part == 0) {
        // The burning grass itself, a low glow that joins the flames up
        // along the front, at 8 fps. It follows the grid blurred, so cells
        // just catching and the unburnt gaps between burning ones glow too
        // and the front reads as one ragged line rather than separate sparks.
        float spread = mix(texelFetch(uGlowPrev, cell, 0).r, texelFetch(uGlow, cell, 0).r, uBlend);
        float glow = max(fire, s.g > 0.05 ? min(1.0, 1.8 * spread) : 0.0);
        if (glow < 0.1) {
            drop();
            return;
        }
        float t = floor(uTime * 8.0) / 8.0;
        float swell = 0.85 + 0.15 * sin(t * 9.0 + h0 * 6.3);
        emit(base + jitter * 0.5 + vec3(0.0, 0.06, 0.0), (0.13 + 0.04 * glow) * swell, 0.15 + 0.5 * glow, 0.0, 1.0);
        return;
    }
    if (part < 3) {
        if (fire < 0.03) {
            drop();
            return;
        }
        // Flames, at 12 fps: each rises off the grass, shrinking and cooling
        // as it goes, so the tops break up into separate tongues.
        float t = floor(uTime * 12.0) / 12.0;
        float life = 0.45 + 0.3 * h1;
        float age = fract(t / life + h0);
        vec3 pos = base + jitter + vec3(0.0, 0.08 + age * 0.7 * fire, 0.0) + wind * 0.15 * age;
        float radius = mix(0.17, 0.05, age) * (0.6 + 0.4 * fire);
        emit(pos, radius, fire * mix(1.5, 0.35, age), 1.0, 1.0);
    } else if (part == 3) {
        // Smoke from about one cell in four, at 6 fps, drifting off downwind.
        if (fire < 0.05 || h1 > 0.25) {
            drop();
            return;
        }
        float t = floor(uTime * 6.0) / 6.0;
        float age = fract(t / 2.5 + h0);
        vec3 pos = base + jitter + vec3(0.0, 0.7 + age * 2.2, 0.0) + wind * 0.7 * age;
        emit(pos, mix(0.12, 0.3, age), 0.5 * fire * (1.0 - age), 3.0, 1.0);
    } else {
        // Embers at 10 fps: sparks flying up off the flames, then a few red
        // pixels glowing on the burnt ground, flickering as they cool.
        float t = floor(uTime * 10.0) / 10.0;
        if (fire > 0.1 && h1 < 0.12) {
            float age = fract(t / 0.9 + h0);
            vec3 pos = base + jitter + vec3(0.0, 0.3 + age * 1.3, 0.0) + wind * 0.4 * age
                     + vec3(sin(age * 9.0 + h2 * 6.3), 0.0, cos(age * 7.0 + h3 * 6.3)) * 0.08;
            emit(pos, 0.0, 0.9 * (1.0 - age), 2.0, 1.0);
        } else if (ember > 0.25 && fire < 0.3 && h1 > 0.88) {
            float on = step(0.35, hash(seed + floor(t * 3.0)));
            if (on <= 0.0) {
                drop();
                return;
            }
            emit(base + jitter + vec3(0.0, 0.02, 0.0), 0.0, ember, 2.0, 1.0);
        } else {
            drop();
        }
    }
}
