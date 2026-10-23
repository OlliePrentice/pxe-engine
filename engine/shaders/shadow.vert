#version 410 core

layout(location = 0) in vec3 aPos;
#ifdef FIRE_GRID
layout(location = 2) in float aSurface;
out vec3 vWorld;
flat out int vSurface;
#endif
// Characters, placed by their joints as in mesh.vert.
layout(location = 3) in uvec4 aJoints;
layout(location = 4) in vec4 aWeights;

uniform mat4 uLightViewProj;
const int kMaxJoints = 64;  // pxe::kMaxJoints
uniform int uSkinned;
uniform mat4 uJoints[kMaxJoints];

void main() {
    vec3 pos = aPos;
    if (uSkinned == 1) {
        mat4 skin = uJoints[aJoints.x] * aWeights.x + uJoints[aJoints.y] * aWeights.y +
                    uJoints[aJoints.z] * aWeights.z + uJoints[aJoints.w] * aWeights.w;
        pos = vec3(skin * vec4(aPos, 1.0));
    }
#ifdef FIRE_GRID
    vWorld = pos;
    vSurface = int(aSurface + 0.5);
#endif
    gl_Position = uLightViewProj * vec4(pos, 1.0);
}
