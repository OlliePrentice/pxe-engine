#version 410 core

// One step of the grass fire, per cell of the grid over the ground. State:
// r how hard it burns (0 not .. 1 full), g dry grass left, b wetness, a how
// hot its embers still are once the flames have gone.

uniform sampler2D uState;   // the latest step
uniform sampler2D uGround;  // r fuel, g roofed, b ground height
uniform float uDt;
uniform float uRain;        // 0 dry .. 1 downpour
uniform vec2 uWind;         // world xz the wind blows toward, times its strength
uniform float uTick;        // steps so far, for the dice
uniform int uIgniteCount;
uniform vec2 uIgnite[8];    // cells to set alight, in cell units
uniform int uStrokeCount;
uniform vec4 uStroke[8];       // brush strokes, from xy to zw, in cell units
uniform vec2 uStrokeShape[8];  // radius in cells, kind: 0 fire, 1 water, 2 regrowth

out vec4 oState;

// Grid x runs along world x and grid y along world z, so the wind is in
// cell directions already.
float dice(vec2 c, float t) {
    vec3 p = fract(vec3(c, t) * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

// How much of a brush stroke covers a point: 1 inside, easing to 0 at its rim.
float strokeCover(vec2 p, vec4 stroke, float radius) {
    vec2 a = stroke.xy, ab = stroke.zw - stroke.xy;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
    return 1.0 - smoothstep(0.6 * radius, radius, distance(p, a + ab * t));
}

void main() {
    ivec2 c = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(uState, 0);
    vec4 s = texelFetch(uState, c, 0);
    vec4 g = texelFetch(uGround, c, 0);
    float fire = s.r, fuel = s.g, wet = s.b, ember = s.a;
    bool roofed = g.g > 0.5;

    // Heat reaching this cell from the burning cells round it, falling off
    // with distance, more from upwind and less from downwind.
    float wl = length(uWind);
    vec2 wd = wl > 1e-4 ? uWind / wl : vec2(0.0);
    float heat = 0.0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x) {
            ivec2 q = c + ivec2(x, y);
            if ((x == 0 && y == 0) || any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, size))) continue;
            float f = texelFetch(uState, q, 0).r;
            if (f <= 0.0) continue;
            vec2 d = vec2(x, y);
            float r2 = dot(d, d);
            float upwind = -dot(d, wd) * inversesqrt(r2);
            heat += f / r2 * max(0.1, 1.0 + 1.2 * wl * upwind);
        }

    // Rain soaks every cell open to the sky; once it stops they dry slowly,
    // faster beside a fire.
    if (!roofed) wet = min(1.0, wet + uDt * uRain * 0.6);
    wet = max(0.0, wet - uDt * (0.004 + 0.03 * heat) * (1.0 - uRain));

    // Dry grass catches at random, the more heat the sooner; wet grass not at all.
    float dry = clamp(1.0 - wet / 0.3, 0.0, 1.0);
    if (fire <= 0.0 && fuel > 0.1 && dice(vec2(c), uTick) < heat * 0.25 * uDt * dry * g.r) fire = 0.12;
    for (int i = 0; i < uIgniteCount; ++i)
        if (distance(vec2(c) + 0.5, uIgnite[i]) < 2.0 && fuel > 0.1 && wet < 0.5) fire = max(fire, 0.5);

    // Brushes: fire dries the grass and sets it alight in scattered cells
    // that soon join up; water soaks it and puts fire and embers out;
    // regrowth brings the grass back and smothers what burns there.
    for (int i = 0; i < uStrokeCount; ++i) {
        float w = strokeCover(vec2(c) + 0.5, uStroke[i], uStrokeShape[i].x);
        if (w <= 0.0) continue;
        int kind = int(uStrokeShape[i].y + 0.5);
        if (kind == 0) {
            wet = max(0.0, wet - w * uDt * 2.0);
            if (fire <= 0.0 && fuel > 0.1 && wet < 0.5 && dice(vec2(c), uTick + 19.0 + float(i)) < w * 0.6)
                fire = 0.5;
        } else if (kind == 1) {
            wet = max(wet, w);
            fire = max(0.0, fire - w * uDt * 6.0);
            ember = max(0.0, ember - w * uDt * 3.0);
        } else {
            fuel = min(g.r, fuel + w * uDt * 1.2 * g.r);
            fire = max(0.0, fire - w * uDt * 6.0);
            ember = max(0.0, ember - w * uDt * 6.0);
        }
    }

    // A burning cell flares up, eats its grass and dies down as the grass
    // runs out; rain and wet ground put it out.
    if (fire > 0.0) {
        fire = min(fire + uDt * 1.5, min(1.0, fuel * 3.0));
        fuel = max(0.0, fuel - uDt * 0.55 * fire);
        fire -= uDt * wet * 3.0;
        ember = max(ember, fire);
        if (fire < 0.02) fire = 0.0;
    }
    // Embers glow on for a while after, and go out at once in the wet.
    ember = max(0.0, ember - uDt * (0.12 + 0.8 * wet));
    oState = vec4(fire, fuel, wet, ember);
}
