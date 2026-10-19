#version 410 core

// The fire grid blurred, for the low glow over burning ground and the
// embers left behind it, and to join up the flames along the front: per
// cell, a binomial blur over the 5x5 cells round it of r fire, g embers,
// b ground height times a, and a how much of it has grass (so b / a is the
// ground's height, even after the texture is filtered).

uniform sampler2D uState;
uniform sampler2D uGround;

out vec4 oGlow;

void main() {
    ivec2 c = ivec2(gl_FragCoord.xy);
    ivec2 top = textureSize(uState, 0) - 1;
    const float kWeight[5] = float[5](1.0, 4.0, 6.0, 4.0, 1.0);
    vec4 sum = vec4(0.0);
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x) {
            ivec2 q = clamp(c + ivec2(x, y), ivec2(0), top);
            vec4 s = texelFetch(uState, q, 0);
            vec4 g = texelFetch(uGround, q, 0);
            float grass = g.r > 0.0 ? 1.0 : 0.0;
            sum += kWeight[x + 2] * kWeight[y + 2] * vec4(s.r, s.a, g.b * grass, grass);
        }
    oGlow = sum / 256.0;
}
