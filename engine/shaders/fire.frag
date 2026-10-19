#version 410 core

flat in float vHeat;
flat in float vKind;

// r: flame heat, summed over every disc covering the pixel; g: ember glow;
// b: smoke density; a: firefly glow.
out vec4 oHeat;

void main() {
    if (vKind > 1.5 && vKind < 2.5) {
        oHeat = vec4(0.0, vHeat, 0.0, 0.0);
        return;
    }
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(d, d);
    if (r2 > 1.0) discard;
    if (vKind > 3.5) {
        oHeat = vec4(0.0, 0.0, 0.0, vHeat);
        return;
    }
    float v = vHeat * (1.0 - r2);
    oHeat = vKind > 2.5 ? vec4(0.0, 0.0, v, 0.0) : vec4(v, 0.0, 0.0, 0.0);
}
