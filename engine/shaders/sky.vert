#version 410 core

// A pane of sky behind each of a room's windows (pxe::Scene::windows), in
// world space, just outside the wall.
layout(location = 0) in vec3 aPos;

uniform mat4 uViewProj;

void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
