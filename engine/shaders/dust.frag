#version 410 core

// An airborne dust mote (dust.vert): its colour and alpha. Colour attachment
// 1 (the normals) is not written.
in vec4 vColour;
layout(location = 0) out vec4 oColour;

void main() {
    oColour = vColour;
}
