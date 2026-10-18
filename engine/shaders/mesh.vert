#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aSurface;

uniform mat4 uViewProj;
// Identity, or a mirror about the water plane for the reflection pass. Only
// the projected position is mirrored; lighting still uses the real point.
uniform mat4 uMirror;
uniform float uClipBelow;  // reflection pass: drop anything under the water

out vec3 vWorld;
out vec3 vNormal;
flat out int vSurface;

void main() {
    vWorld = aPos;
    vNormal = aNormal;
    vSurface = int(aSurface + 0.5);
    gl_Position = uViewProj * uMirror * vec4(aPos, 1.0);
    gl_ClipDistance[0] = aPos.y - uClipBelow;
}
