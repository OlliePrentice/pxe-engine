#version 410 core

in vec2 vLocal;
flat in vec3 vCol;
flat in vec3 vTip;
flat in float vKind;
flat in float vSeed;
flat in float vScale;
flat in float vSway;

uniform int uMagenta;
uniform sampler2D uGround;  // terrain colour, copied before grass draws

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;  // sprites take no edges

float hash(float i) {
    return fract(sin((vSeed * 113.0 + i) * 12.9898) * 43758.5453);
}

// One blade rasterised at pixel centre p. Grows from (baseX, 0) to height h,
// curving by `lean` pixels at the tip and tapering from 2*rootW pixels to
// one, so the edge it leaves on the next band is a clean saw-tooth. Returns
// 0 for a miss, 1 for the body, 2 for the top pixel.
int blade(vec2 p, float baseX, float h, float lean, float rootW) {
    if (p.y > h) return 0;
    float t = p.y / h;
    float xc = baseX + lean * t * t;
    float hw = mix(rootW, 0.5, t);
    if (abs(p.x - xc) >= hw) return 0;
    return (p.y + 1.0 > h) ? 2 : 1;
}

// A leaf: a small ellipse of `len` by `wid` pixels, centred at c, pointing
// along angle a.
bool leafShape(vec2 p, vec2 c, float a, float len, float wid) {
    vec2 d = p - c;
    vec2 u = vec2(cos(a), sin(a));
    float x = dot(d, u) / (len * 0.5);
    float y = dot(d, vec2(-u.y, u.x)) / (wid * 0.5);
    return x * x + y * y <= 1.0;
}

void main() {
    vec2 p = floor(vLocal) + 0.5;
    int hit = 0;

    if (vKind > 1.5) {
        // Leaf cluster: two or three leaves fanned around the root, the
        // whole cluster nudged by the wind.
        int n = hash(1.0) < 0.5 ? 2 : 3;
        float base = hash(2.0) * 6.2831;
        for (int i = 0; i < 3; ++i) {
            if (i >= n) break;
            float a = base + float(i) * 6.2831 / float(n) + (hash(float(i) + 3.0) - 0.5);
            vec2 c = vec2(cos(a), sin(a)) * 1.6 + vec2(0.5 + vSway * 0.5, 0.5);
            if (leafShape(p, c, a, mix(3.2, 4.6, hash(float(i) + 7.0)) * vScale, 2.2)) hit = 1;
        }
    } else if (vKind < 0.5) {
        // Clump: three short, near-vertical blades, the middle one tallest.
        for (int i = 0; i < 3; ++i) {
            float fi = float(i);
            float side = fi - 1.0;
            float baseX = side * 2.0 + (hash(fi) - 0.5) + 0.5;
            float h = (side == 0.0 ? mix(5.0, 8.0, hash(fi + 3.0)) : mix(2.5, 5.0, hash(fi + 3.0))) * vScale;
            float lean = side * mix(0.3, 1.0, hash(fi + 6.0)) + vSway;
            hit = max(hit, blade(p, baseX, h, lean, 1.6));
        }
    } else {
        // Tuft: a small fan whose outer blades splay out to a V.
        int n = hash(9.0) < 0.35 ? 2 : 3;
        for (int i = 0; i < 3; ++i) {
            if (i >= n) break;
            float side = (n == 2) ? (float(i) * 2.0 - 1.0) : (float(i) - 1.0);
            float baseX = side * 0.75 + 0.5;
            float h = (side == 0.0 ? 8.0 : 6.0) * mix(0.85, 1.15, hash(float(i) + 11.0)) * vScale;
            float lean = side * mix(3.0, 4.5, hash(float(i) + 14.0)) + 1.5 + 0.6 * vSway;
            hit = max(hit, blade(p, baseX, h, lean, 1.0));
        }
    }

    if (hit == 0) discard;
    oNormal = vec4(0.5, 1.0, 0.5, 0.0);
    if (uMagenta == 1) {
        oColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }
    // A clump's top pixel catches the light where it sticks out over darker
    // ground, which is what makes the fringe read against a shadow edge.
    vec3 col = vCol;
    if (vKind < 0.5 && hit == 2) {
        vec3 behind = texelFetch(uGround, ivec2(gl_FragCoord.xy), 0).rgb;
        const vec3 kLuma = vec3(0.299, 0.587, 0.114);
        if (dot(behind, kLuma) + 0.04 < dot(vCol, kLuma)) col = vTip;
    }
    oColor = vec4(col, 1.0);
}
