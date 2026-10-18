#version 410 core

// Distance from the fire to the nearest surface, per direction.

in vec3 vWorld;

uniform vec3 uLightPos;

out vec4 oDistance;

void main() {
    oDistance = vec4(length(vWorld - uLightPos));
}
