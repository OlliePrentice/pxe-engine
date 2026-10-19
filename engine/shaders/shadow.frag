#version 410 core

#ifdef FIRE_GRID
// While the grass fire runs, a crown burning away lets the light through
// its holes, the same holes mesh.frag cuts (crownGone in common.glsl).
in vec3 vWorld;
flat in int vSurface;
uniform sampler2D uBurn;
uniform vec4 uBurnDomain;

void main() {
    if (vSurface != 4) return;
    vec2 uv = (vWorld.xz - uBurnDomain.xy) * uBurnDomain.zw;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return;
    float burnt = textureLod(uBurn, uv, 0.0).g;
    vec3 q = floor(vWorld * 3.0);
    if (burnt > 0.5 + 0.45 * fract(sin(dot(q, vec3(12.9898, 78.233, 37.719))) * 43758.5453)) discard;
}
#else
void main() {}
#endif
