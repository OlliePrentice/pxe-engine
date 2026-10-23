#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aSurface;
// Characters only (pxe::SkinVertex): the joints that carry the vertex and
// their weights.
layout(location = 3) in uvec4 aJoints;
layout(location = 4) in vec4 aWeights;

uniform mat4 uViewProj;
// Identity, or a mirror about the water plane for the reflection pass. Only
// the projected position is mirrored; lighting still uses the real point.
uniform mat4 uMirror;
uniform float uClipBelow;  // reflection pass: drop anything under the water
// 1 while a character draws: its vertices are at rest in its model, and
// uJoints holds its joint matrices (pxe::SkinnedModel::skin).
const int kMaxJoints = 64;  // pxe::kMaxJoints
uniform int uSkinned;
uniform mat4 uJoints[kMaxJoints];

out vec3 vWorld;
out vec3 vNormal;
flat out int vSurface;

void main() {
    vec3 pos = aPos;
    vec3 normal = aNormal;
    if (uSkinned == 1) {
        mat4 skin = uJoints[aJoints.x] * aWeights.x + uJoints[aJoints.y] * aWeights.y +
                    uJoints[aJoints.z] * aWeights.z + uJoints[aJoints.w] * aWeights.w;
        pos = vec3(skin * vec4(aPos, 1.0));
        normal = normalize(mat3(skin) * aNormal);
    }
    vWorld = pos;
    vNormal = normal;
    vSurface = int(aSurface + 0.5);
    gl_Position = uViewProj * uMirror * vec4(pos, 1.0);
    gl_ClipDistance[0] = pos.y - uClipBelow;
}
