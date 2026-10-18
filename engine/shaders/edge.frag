#version 410 core

// Pixel-art edge pass over the low-res target, after David Holland's write-up
// of t3ssel8r's look. Only stone and bark take edges. A pixel on an upward
// face whose neighbour turns away (a crease) or drops behind (a silhouette)
// catches a light rim; any other silhouette pixel darkens into an outline.

uniform sampler2D uColor;
uniform sampler2D uNormal;  // xyz normal * 0.5 + 0.5, a = (surface + 1) / 8, 0 for sprites
uniform sampler2D uDepth;
uniform float uDepthMetres;  // far - near: the ortho depth buffer is linear
uniform int uEnabled;

const int kSurfStone = 2;
const int kSurfBark = 3;
const float kSilhouette = 0.35;  // metres of depth step that count as an outline
uniform vec3 uLight;  // toward the sun or moon
uniform vec3 uRim;  // blended with the time of day
uniform vec3 uInk;

out vec4 oColor;

void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec3 col = texelFetch(uColor, p, 0).rgb;
    vec4 np = texelFetch(uNormal, p, 0);
    int surface = int(np.a * 8.0 + 0.5) - 1;
    if (uEnabled == 0 || (surface != kSurfStone && surface != kSurfBark)) {
        oColor = vec4(col, 1.0);
        return;
    }
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
        bool crease = !behind && abs(dq - d) < kSilhouette && nq4.a > 0.0 && dot(n, nq) < 0.8;
        if (n.y > 0.6 && (behind || (crease && nq.y < n.y - 0.3))) rim = true;
        else if (behind && !diagonal) ink = true;
        // Bark is smooth-shaded except along a few seams marked sharp in
        // Blender; there the normal jumps a little, and one light stroke is
        // drawn on the side that faces the light more.
        if (surface == kSurfBark && !behind && !diagonal && abs(dq - d) < kSilhouette && nq4.a > 0.0 &&
            dot(n, nq) < 0.97 && dot(n, uLight) > dot(nq, uLight)) stroke = true;
    }
    // Bark is painted, not carved: no rims, a faint outline, and its few
    // seam strokes are soft.
    if (surface == kSurfBark) rim = false;
    if (stroke) col = mix(col, uRim, 0.15);
    else if (rim) col = mix(col, uRim, 0.4);
    else if (ink) col = mix(col, uInk, surface == kSurfBark ? 0.15 : 0.35);
    oColor = vec4(col, 1.0);
}
