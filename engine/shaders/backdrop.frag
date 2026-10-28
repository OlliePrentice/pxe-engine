#version 410 core

// The sky behind a perspective view (pxe::FirstPersonCamera), drawn over the
// whole target at the far plane (depth 1, tested less-or-equal) after the
// meshes, so it fills only the empty background. A vertical gradient from
// the horizon colour to the zenith colour by how far each pixel's view ray
// climbs above the horizon, cut into flat bands like the toon surfaces;
// below the horizon it is the horizon colour, which far surfaces fade into
// (the edge pass's haze). No sun or moon disc, stars or clouds. With the
// normal target left alone, the edge pass treats it as the background: no
// outline, light or haze on it.
uniform vec2 uTarget;   // the low-res target, pixels
uniform vec3 uFwd;      // the camera's basis
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec2 uTanHalf;  // tangents of half the horizontal and vertical field of view
uniform vec3 uHorizon;
uniform vec3 uZenith;

layout(location = 0) out vec4 oColor;

const float kBands = 6.0;

void main() {
    vec2 s = (gl_FragCoord.xy / uTarget * 2.0 - 1.0) * uTanHalf;
    vec3 ray = normalize(uFwd + uRight * s.x + uUp * s.y);
    // The square root of the ray's climb spreads the bands thin near the
    // horizon, where a real sky changes fastest, and wide overhead.
    float t = sqrt(clamp(ray.y, 0.0, 1.0));
    float band = min(floor(t * kBands), kBands - 1.0) / (kBands - 1.0);
    oColor = vec4(mix(uHorizon, uZenith, band), 1.0);
    gl_FragDepth = 1.0;
}
