#version 410 core

// The sky behind a perspective view (pxe::FirstPersonCamera), drawn over the
// whole target at the far plane (depth 1, tested less-or-equal) after the
// meshes, so it fills only the empty background. A continuous gradient from
// the horizon colour to the zenith colour by how far each pixel's view ray
// climbs above the horizon; below the horizon it is the horizon colour, which
// far surfaces fade into (the edge pass's haze). The sky takes the sun's
// colour in a glow round the sun, the same glow the haze has, and the sun
// itself is a small disc with a soft edge. Over the glow and the disc go two
// layers of cloud (Scene::skyClouds): soft streaks stretched along the wind
// on flat planes at altitude, coloured from the sky and the sun, thinning
// into the horizon; they cast no shadows. The moon has no disc, and there
// are no stars. With the normal target left alone, the edge pass
// treats it as the background: no outline, light or haze on it.
uniform vec2 uTarget;   // the low-res target, pixels
uniform vec3 uFwd;      // the camera's basis
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec2 uTanHalf;  // tangents of half the horizontal and vertical field of view
uniform vec3 uHorizon;
uniform vec3 uZenith;
uniform vec3 uSunDir;   // toward the sun
uniform vec3 uSunColour;  // its display colour
uniform float uSunGlow;   // 0..1, already faded by overcast
uniform float uOvercastDisc;  // 0..1: rain cloud hides the disc
uniform vec3 uEye;      // the camera's position
uniform vec2 uWindDir;  // unit, world XZ: the way the streaks run
uniform float uCloudHeight[2], uCloudCover[2], uCloudOpacity[2], uCloudLength[2], uCloudWidth[2];
uniform vec2 uCloudOffset[2];  // metres each layer has drifted
uniform float uCloudBrightness;

layout(location = 0) out vec4 oColor;

const float kGlowPower = 8.0;          // how tight the glow round the sun is
const float kDiscRadius = 0.0140;      // radians, about 0.8 degrees: ~7 px tall at a 60 degree view
const float kDiscSoft = 0.0052;        // radians of soft edge, about 0.3 degrees

// The glow toward the sun, 0..1, fading out as the sun sets below the horizon.
float sunGlowAt(vec3 ray) {
    float toward = max(dot(ray, uSunDir), 0.0);
    return clamp(uSunGlow * pow(toward, kGlowPower) * smoothstep(-0.10, 0.05, uSunDir.y), 0.0, 1.0);
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

// Four octaves, amplitude 0.5 halving, frequency doubling, offset (17.1, 9.7)
// per octave: about 0.2..0.8.
float cloudFbm(vec2 p) {
    float n = 0.0, amp = 0.5;
    for (int i = 0; i < 4; ++i) {
        n += amp * cloudNoise(p);
        p = p * 2.0 + vec2(17.1, 9.7);
        amp *= 0.5;
    }
    return n;
}

// One layer over `sky` along view ray `ray` (ray.y > 0 only).
vec3 cloudLayer(vec3 sky, vec3 ray, int i) {
    if (uCloudCover[i] <= 0.0 || ray.y <= 0.0) return sky;
    float t = uCloudHeight[i] / max(ray.y, 0.02);
    vec2 p = uEye.xz + ray.xz * t + uCloudOffset[i];
    vec2 across = vec2(-uWindDir.y, uWindDir.x);
    vec2 q = vec2(dot(p, uWindDir) / uCloudLength[i], dot(p, across) / uCloudWidth[i]);
    float n = cloudFbm(q + float(i) * vec2(31.7, 11.3));
    float edge = mix(0.75, 0.25, uCloudCover[i]);
    float d = smoothstep(edge, edge + 0.15, n) * smoothstep(0.0, 0.12, ray.y);
    if (d <= 0.0) return sky;
    float thick = clamp((n - edge) / 0.3, 0.0, 1.0);
    float toward = pow(max(dot(ray, uSunDir), 0.0), 3.0) * smoothstep(-0.10, 0.05, uSunDir.y);
    vec3 lit = mix(uHorizon * 1.15, uSunColour, toward);
    vec3 under = mix(uHorizon, uZenith, 0.5) * 0.85;
    vec3 cloud = min(mix(lit, under, 0.5 * thick) * uCloudBrightness, vec3(1.0));
    return mix(sky, cloud, d * uCloudOpacity[i]);
}

void main() {
    vec2 s = (gl_FragCoord.xy / uTarget * 2.0 - 1.0) * uTanHalf;
    vec3 ray = normalize(uFwd + uRight * s.x + uUp * s.y);
    // The square root of the ray's climb spreads the change thin near the
    // horizon, where a real sky changes fastest, and wide overhead.
    vec3 sky = mix(uHorizon, uZenith, sqrt(clamp(ray.y, 0.0, 1.0)));
    sky = mix(sky, uSunColour, sunGlowAt(ray));
    float angle = acos(clamp(dot(ray, uSunDir), -1.0, 1.0));
    float disc = (1.0 - smoothstep(kDiscRadius, kDiscRadius + kDiscSoft, angle)) * step(-0.02, uSunDir.y) * (1.0 - uOvercastDisc);
    sky = mix(sky, min(uSunColour * 1.3 + 0.15, vec3(1.0)), disc);
    sky = cloudLayer(sky, ray, 0);
    sky = cloudLayer(sky, ray, 1);
    oColor = vec4(sky, 1.0);
    gl_FragDepth = 1.0;
}
