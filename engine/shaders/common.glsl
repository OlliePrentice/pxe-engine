// Shared toon ramp. Spliced into the mesh fragment shader and the grass
// vertex shader so both run the same function on the same kind of inputs.
const int kSurfaces = 7;
const int kSurfWater = 5;
const int kSurfFoam = 6;

uniform vec3 uSunDir;
uniform float uTLit;
uniform float uTMid;
uniform vec3 uBand[kSurfaces * 3];  // per surface: shadow, mid, lit
uniform mat4 uLightViewProj;
uniform sampler2DShadow uShadowMap;
uniform float uNormalBias;

float sunVisible(vec3 wpos, vec3 n) {
    vec4 ls = uLightViewProj * vec4(wpos + n * uNormalBias, 1.0);
    vec3 p = ls.xyz / ls.w * 0.5 + 0.5;
    return texture(uShadowMap, vec3(p.xy, p.z));
}

// 0 shadow, 1 mid, 2 lit.
int toonBand(vec3 wpos, vec3 n) {
    float ndl = dot(normalize(n), uSunDir);
    if (ndl <= uTMid || sunVisible(wpos, n) < 0.5) return 0;
    return (ndl > uTLit) ? 2 : 1;
}

vec3 bandColour(int surface, int band) {
    return uBand[surface * 3 + band];
}
