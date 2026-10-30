#version 410 core
uniform mat4 uViewProj;
uniform vec3 uEye;
uniform vec3 uOffset;     // metres the wind has carried the dust
uniform float uTime;      // FrameParams::time
uniform float uSize;      // Scene::dustSize
uniform vec3 uHaze;       // the haze colour (as the edge pass's uHaze)
uniform vec3 uSunDir;     // toward the sun
uniform vec3 uSunColour;  // its glow colour
uniform float uSunGlow;   // as the sky's, already faded by overcast
out vec4 vColour;

const float kReach = 40.0;    // the box reaches this far from the eye each way
const float kFadeFrom = 10.0; // full until here, nothing at kReach
const float kNear = 4.0;      // nearer than this, a mote is uSize pixels

float hash(uint n) {  // integer hash to [0, 1)
    n = (n ^ 61u) ^ (n >> 16); n *= 9u; n ^= n >> 4; n *= 0x27d4eb2du; n ^= n >> 15;
    return float(n & 0xffffffu) / 16777216.0;
}

void main() {
    uint i = uint(gl_VertexID);
    vec3 seed = vec3(hash(3u * i), hash(3u * i + 1u), hash(3u * i + 2u));
    float span = 2.0 * kReach;
    vec3 wobble = vec3(sin(uTime * 0.7 + seed.x * 6.2832) * 0.3,
                       sin(uTime * 0.5 + seed.y * 6.2832) * 0.15,
                       cos(uTime * 0.6 + seed.z * 6.2832) * 0.3);
    vec3 p = seed * span + uOffset + wobble;
    vec3 rel = mod(p - uEye + kReach, span) - kReach;   // the copy nearest the eye
    float dist = length(rel);
    float fade = 1.0 - smoothstep(kFadeFrom, kReach, dist);
    vec3 dir = rel / max(dist, 1e-3);
    float glint = clamp(uSunGlow * pow(max(dot(dir, uSunDir), 0.0), 6.0) * smoothstep(-0.10, 0.05, uSunDir.y), 0.0, 1.0);
    vColour = vec4(min(mix(uHaze, uSunColour, glint) * 1.15, vec3(1.0)), 0.5 * fade);
    gl_PointSize = dist < kNear ? floor(uSize + 0.5) : 1.0;
    gl_Position = uViewProj * vec4(uEye + rel, 1.0);
    if (fade <= 0.0) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // outside the clip volume
}
