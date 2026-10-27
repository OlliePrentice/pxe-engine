#version 410 core

// The sky seen through a room's window: one flat colour for the time of
// day. The pane is drawn at the far plane (depth 1, tested less-or-equal),
// so it fills only what the opening shows of the empty background - never
// a wall, mullion or anything else drawn - and, with the normal target left
// alone, the edge pass treats it exactly as that background: no outline,
// light or rain changes on or round it.
uniform vec3 uSky;

layout(location = 0) out vec4 oColor;

void main() {
    oColor = vec4(uSky, 1.0);
    gl_FragDepth = 1.0;
}
