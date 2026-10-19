#version 410 core

// Per patch of cells: its summed fire, the fire-weighted world x and z (for
// where its light sits), and its summed embers; and for the crowns over
// it, their summed fire and its fire-weighted x, z and height.

uniform sampler2D uState;
uniform sampler2D uCanopy;
uniform sampler2D uCanopyGround;
uniform vec2 uMin;
uniform float uCell;
uniform int uPatch;

layout(location = 0) out vec4 oSum;
layout(location = 1) out vec4 oCrown;

void main() {
    ivec2 first = ivec2(gl_FragCoord.xy) * uPatch;
    ivec2 size = textureSize(uState, 0);
    vec4 sum = vec4(0.0), crown = vec4(0.0);
    for (int y = 0; y < uPatch; ++y)
        for (int x = 0; x < uPatch; ++x) {
            ivec2 c = first + ivec2(x, y);
            if (c.x >= size.x || c.y >= size.y) continue;
            vec4 s = texelFetch(uState, c, 0);
            float f = texelFetch(uCanopy, c, 0).r;
            vec2 world = uMin + (vec2(c) + 0.5) * uCell;
            sum += vec4(s.r, s.r * world, s.a);
            crown += f * vec4(1.0, world, texelFetch(uCanopyGround, c, 0).g);
        }
    oSum = sum;
    oCrown = crown;
}
