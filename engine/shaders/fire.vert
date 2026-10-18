#version 410 core

// Fire particles as points into the heat buffer, after t3ssel8r's fire:
// each is a disc snapped to the pixel grid, sized by its radius in metres.
// Embers are a single pixel.

layout(location = 0) in vec4 aPart;  // world xyz, radius in metres
layout(location = 1) in vec2 aHeat;  // heat or density, kind (0 core, 1 flame, 2 ember, 3 smoke)

uniform mat4 uViewProj;
uniform vec2 uTarget;           // render target size in pixels
uniform float uPxPerMetre;
uniform float uDepthBiasNdc;    // pulls the fire toward the camera through its own logs a little

flat out float vHeat;
flat out float vKind;

void main() {
    vec4 c = uViewProj * vec4(aPart.xyz, 1.0);
    bool ember = aHeat.y > 1.5 && aHeat.y < 2.5;
    float size = ember ? 1.0 : max(1.0, floor(2.0 * aPart.w * uPxPerMetre + 0.5));
    // Odd sizes centre on a pixel, even ones on a pixel corner, so every
    // disc covers whole pixels.
    vec2 px = (c.xy / c.w * 0.5 + 0.5) * uTarget;
    px = mod(size, 2.0) > 0.5 ? floor(px) + 0.5 : floor(px + 0.5);
    gl_Position = vec4(px / uTarget * 2.0 - 1.0, c.z / c.w - uDepthBiasNdc, 1.0);
    gl_PointSize = size;
    vHeat = aHeat.x;
    vKind = aHeat.y;
}
