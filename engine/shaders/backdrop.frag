#version 410 core

// The sky behind a perspective view (pxe::FirstPersonCamera), drawn over the
// whole target at the far plane (depth 1, tested less-or-equal) after the
// meshes, so it fills only the empty background. A continuous gradient from
// the horizon colour to the zenith colour by how far each pixel's view ray
// climbs above the horizon; below the horizon it is the horizon colour, which
// far surfaces fade into (the edge pass's haze). The sky takes the sun's
// colour in a glow round the sun, the same glow the haze has, and the sun
// itself is a small disc with a soft edge. The moon has no disc, and there
// are no stars or clouds. With the normal target left alone, the edge pass
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

layout(location = 0) out vec4 oColor;

const float kGlowPower = 8.0;          // how tight the glow round the sun is
const float kDiscRadius = 0.0140;      // radians, about 0.8 degrees: ~7 px tall at a 60 degree view
const float kDiscSoft = 0.0052;        // radians of soft edge, about 0.3 degrees

// The glow toward the sun, 0..1, fading out as the sun sets below the horizon.
float sunGlowAt(vec3 ray) {
    float toward = max(dot(ray, uSunDir), 0.0);
    return clamp(uSunGlow * pow(toward, kGlowPower) * smoothstep(-0.10, 0.05, uSunDir.y), 0.0, 1.0);
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
    oColor = vec4(sky, 1.0);
    gl_FragDepth = 1.0;
}
