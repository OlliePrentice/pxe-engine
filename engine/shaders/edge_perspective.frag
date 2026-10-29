#version 410 core

// The edge pass under a perspective camera (RenderMode::kPerspective,
// pxe::FirstPersonCamera): a post pass over the low-res target that draws
// outlines, rims, creases and bark strokes from the perspective depth
// buffer, and fades distant surfaces into the sky's horizon colour.
//
// Edges, after David Holland's write-up of t3ssel8r's look: only stone and
// bark take them, and a building's wood, plaster, roof and oak. A pixel on an
// upward face whose neighbour turns away (a crease) or drops behind (a
// silhouette) catches a light rim; any other silhouette pixel darkens into
// an outline. Wood, plaster, roof and oak take no rims, only the outline, on
// every silhouette: indoors they are seen in shade, where a light line on
// every plank groove would be noise. Indoors, oak is inked along its sharp
// creases too, so a bar top reads against its front; wood is not, so the
// floor's plank grooves stay unlined. Outdoors the roof's slates are inked
// the same way, so each course and tile joint reads. A character's creased
// materials (a door's planks and grain) are inked so too, indoors and out;
// otherwise they are stone, as every character is. Here world positions come
// from the perspective depth buffer, and the depth steps that count as an
// outline or a crease grow with distance and with how steeply the surface
// runs away from the eye.
//
// Dark regions (pxe::Scene::darkRegions): a light rim inside one takes the
// region's share of the rim colour.
//
// The view model (pxe::FrameParams::viewModel): its pixels are read at
// their true depth, though it is drawn squeezed in front of the scene. Each
// of its silhouettes, against the scene and where one of its parts stands
// more than kViewModelStep in front of another, is inked dark, and it takes
// a light rim only at an upward crease. The scene never outlines against
// it. A frame with no view model is drawn exactly as before.
//
// Distance haze: surfaces fade into the sky's horizon colour toward the far
// plane, in flat steps like the sky's bands.
//
// This is a fork of edge.frag, the orthographic mode's edge pass, kept
// separate so that tuning either mode's never changes the other's. A change
// to a rule both share (rim, ink, crease) is made in each file on its own,
// on purpose.

uniform sampler2D uColor;
uniform sampler2D uNormal;  // xyz normal * 0.5 + 0.5, a = (surface + 1) / kSurfaceScale; 0 where nothing was drawn (the sky)
uniform sampler2D uDepth;
uniform int uEnabled;

const int kSurfStone = 2;
const int kSurfBark = 3;
const int kSurfWood = 7;
const int kSurfRoof = 9;
const int kSurfOak = 10;
// A building's wood, plaster, roof and oak: outlined, never rimmed (edges()).
bool timbered(int surface) { return (surface >= kSurfWood && surface <= kSurfRoof) || surface == kSurfOak; }
const float kSurfaceScale = 16.0;  // as common.glsl
const int kSurfActorCrease = 12;   // an actor's creased material (mesh.frag), stone otherwise

// Cut stone and a building's wood, plaster, roof and oak: hard, flat-faced
// structure, rimmed and outlined alike.
bool built(int surface) {
    return surface == kSurfStone || (surface >= kSurfWood && surface <= kSurfRoof) || surface == kSurfOak;
}
const float kSilhouette = 0.35;  // metres of depth step that count as an outline
// Indoors oak, and outdoors a roof's slates, take an ink line where the
// surface turns sharply with no depth step: neighbouring normals more than
// 40 degrees apart. Only those pixels are inked; where oak meets wood,
// plaster or stone, or slate meets the chimney or a gable, the line falls
// on the oak or slate side. An actor's creased material is inked the same
// way, against its own kind (the door's wood against its grain) or, against
// anything else (its iron, the wall), on its side.
uniform int uIndoors;  // 1 in a room (pxe::Scene::indoors)
bool creased(int surface) { return surface == (uIndoors != 0 ? kSurfOak : kSurfRoof); }
const float kCreaseCos = 0.766;  // cos 40 degrees
uniform vec3 uLight;  // toward the sun or moon
uniform vec3 uRim;  // blended with the time of day
uniform vec3 uInk;
// Dark regions (pxe::Scene::darkRegions), as common.glsl has them: a light
// rim inside one takes the region's share of the rim colour.
const int kDarkRegions = 8;  // Renderer: regions past this are left out
uniform int uDarkCount;
uniform vec3 uDarkCentre[kDarkRegions];
uniform vec3 uDarkAxisX[kDarkRegions];
uniform vec3 uDarkAxisZ[kDarkRegions];
uniform vec3 uDarkHalf[kDarkRegions];
uniform vec3 uDarkOpen[kDarkRegions];
uniform vec3 uDarkFade[kDarkRegions];

// As common.glsl's darkAt: the share a point in a dark region takes, -1
// outside every region.
float darkAt(vec3 wpos) {
    float k = -1.0;
    for (int i = 0; i < uDarkCount; ++i) {
        vec3 d = wpos - uDarkCentre[i];
        vec3 l = vec3(dot(d, uDarkAxisX[i]), d.y, dot(d, uDarkAxisZ[i]));
        if (any(greaterThan(abs(l), uDarkHalf[i]))) continue;
        vec3 o = uDarkOpen[i];
        float depth = dot(uDarkHalf[i], abs(o)) - dot(l, o);
        vec3 f = uDarkFade[i];
        float s = f.z > 0.0 ? mix(f.x, f.y, clamp(depth / f.z, 0.0, 1.0)) : f.y;
        k = k < 0.0 ? max(s, 0.0) : min(k, max(s, 0.0));
    }
    return k;
}

uniform vec3 uEye;          // camera, for rebuilding world positions from depth
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec3 uFwd;
uniform float uNear;
// The view model (pxe::FrameParams::viewModel) is drawn into [0,
// uViewModelDepth] of the depth range, in front of the whole scene: a pixel
// whose depth is below it is the view model's, and its true depth is its
// depth stretched back over the whole range. 0 when the frame has none.
uniform float uViewModelDepth;
// Metres of depth step that count as an outline inside the view model.
const float kViewModelStep = 0.01;
uniform float uFar;            // the far plane, metres (uNear is the near one)
uniform vec2 uTanHalf;         // tangents of half the horizontal and vertical field of view
uniform float uPixelAngle;     // metres across one pixel per metre of distance
uniform float uRefTexel;       // metres across one pixel of the orthographic view, which kSilhouette suits
uniform vec3 uHaze;            // the sky's horizon colour
uniform vec2 uHazeSpan;        // distances where the haze starts and is full, metres

out vec4 oColor;

// The world position under a pixel, from depth (below).
vec3 worldAt(ivec2 p);
// The view ray through a pixel, its forward component 1, and the depth of
// the surface there along the camera's forward axis, metres.
vec3 rayAt(ivec2 p) {
    vec2 s = ((vec2(p) + 0.5) / vec2(textureSize(uDepth, 0)) * 2.0 - 1.0) * uTanHalf;
    return uFwd + uRight * s.x + uUp * s.y;
}

bool viewModelAt(ivec2 p) { return texelFetch(uDepth, p, 0).r < uViewModelDepth; }

float viewDepth(ivec2 p) {
    float w = texelFetch(uDepth, p, 0).r;
    if (w < uViewModelDepth) w /= uViewModelDepth;
    float z = w * 2.0 - 1.0;
    return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear));
}

// `actorCrease`: a pixel of an actor's creased material, its surface stone.
vec3 edges(ivec2 p, vec3 col, vec4 np, int surface, bool actorCrease) {
    if (uEnabled == 0 || (!built(surface) && surface != kSurfBark)) return col;
    vec3 n = normalize(np.xyz * 2.0 - 1.0);
    // One pixel spans more of the world the farther it is, so the depth
    // step that counts grows with distance past where a pixel is as wide as
    // the orthographic view's. A surface running away from the eye also
    // steps in depth from pixel to pixel by itself: each neighbour is also
    // allowed how far this pixel's own plane, carried on to the neighbour's
    // view ray, lies from this depth (up to this depth again, for a plane
    // seen nearly edge-on).
    float d = viewDepth(p);
    vec3 fromEye = rayAt(p) * d;
    bool vm = viewModelAt(p);
    float apart = vm ? kViewModelStep : kSilhouette * max(1.0, d * uPixelAngle / uRefTexel);
    vec3 look = normalize(fromEye);

    bool rim = false;
    bool ink = false;
    bool stroke = false;
    bool creaseInk = false;
    bool creases = actorCrease || creased(surface);
    // Diagonal neighbours count for rims, so a seam that rasterises as a
    // 2:1 staircase joins into one line instead of a row of dots.
    const ivec2 kOffsets[8] = ivec2[8](ivec2(0, 1), ivec2(0, -1), ivec2(1, 0), ivec2(-1, 0),
                                      ivec2(1, 1), ivec2(-1, 1), ivec2(1, -1), ivec2(-1, -1));
    for (int i = 0; i < 8; ++i) {
        bool diagonal = i >= 4;
        ivec2 q = p + kOffsets[i];
        bool vmq = viewModelAt(q);
        // A scene pixel ignores the view model in front of it.
        if (!vm && vmq) continue;
        float dq = viewDepth(q);
        vec3 rq = rayAt(q);
        float facing = dot(rq, n);
        float onPlane = facing < -1e-4 ? dot(fromEye, n) / facing : 2.0 * d;
        float tol = apart + min(abs(onPlane - d), d);
        vec4 nq4 = texelFetch(uNormal, q, 0);
        vec3 nq = normalize(nq4.xyz * 2.0 - 1.0);
        // A view-model pixel's neighbour in the scene or sky is always behind it.
        bool behind = (vm && !vmq) || dq - d > tol;
        bool meshNeighbour = nq4.a > 0.0 && nq4.a < 0.99;  // something was drawn there (0 is the sky), and it is no sprite
        bool crease = !behind && abs(dq - d) < tol && meshNeighbour && dot(n, nq) < 0.8;
        if (vm) {
            // The view model: dark outlines on every silhouette, light rims
            // only at upward creases.
            if (n.y > 0.6 && crease && nq.y < n.y - 0.3) rim = true;
            if (behind && !diagonal) ink = true;
        } else if (!timbered(surface) && n.y > 0.6 && (behind || (crease && nq.y < n.y - 0.3))) rim = true;
        else if (behind && !diagonal) ink = true;
        // Bark is smooth-shaded except along a few seams marked sharp in
        // Blender; there the normal jumps a little, and one light stroke is
        // drawn on the side that faces the light more.
        if (surface == kSurfBark && !behind && !diagonal && abs(dq - d) < tol && meshNeighbour &&
            dot(n, nq) < 0.97 && dot(n, uLight) > dot(nq, uLight)) stroke = true;
        // A crease inked once, one pixel wide: where oak meets oak (or
        // slate slate), on the side turned further from the camera (the
        // narrower face on screen), or, where they turn from it alike - the
        // two near sides of a table at the room's 45 degree yaw - on the
        // side facing more toward +x. Against anything else (wood, plaster,
        // stone), on this oak or slate side.
        if (creases && !diagonal && abs(dq - d) < tol && meshNeighbour && dot(n, nq) < kCreaseCos) {
            int sq = int(nq4.a * kSurfaceScale + 0.5) - 1;
            bool partner = actorCrease ? sq == kSurfActorCrease : creased(sq);
            float toCam = dot(nq, look) - dot(n, look);  // < 0: this side faces the camera less
            if (!partner || toCam < -1e-3 || (abs(toCam) <= 1e-3 && n.x > nq.x)) creaseInk = true;
        }
    }
    // A view model's silhouette is inked as a slate crease is, before any rim.
    if (vm && ink) return min(mix(col, uInk, 0.65), col * 0.8);
    // Bark is painted, not carved: no rims, a faint outline, and its few
    // seam strokes are soft.
    if (surface == kSurfBark) rim = false;
    // In a dark region a light line takes the region's share of the rim.
    if (uDarkCount > 0 && (stroke || rim)) {
        float dark = darkAt(worldAt(p));
        if (dark >= 0.0) return mix(col, uRim * dark, stroke ? 0.15 : 0.4);
    }
    if (stroke) return mix(col, uRim, 0.15);
    if (rim) return mix(col, uRim, 0.4);
    if (ink) return mix(col, uInk, surface == kSurfBark ? 0.15 : 0.35);
    // A crease on oak is inked as an outline is. Slate is close to the ink
    // in colour, so its creases take a heavier mix to read as dark lines
    // along every course and tile joint; and slate in its shadow tone is
    // darker than the ink itself, so a slate crease is also kept at least a
    // step darker than the slate it is drawn on. The door's dark wood takes
    // the slate's treatment, indoors and out: at oak's mix it barely shows.
    if (creaseInk) {
        if (uIndoors != 0 && !actorCrease) return mix(col, uInk, 0.35);
        return min(mix(col, uInk, 0.65), col * 0.8);
    }
    return col;
}

// The world position under pixel p.
vec3 worldAt(ivec2 p) { return uEye + rayAt(p) * viewDepth(p); }

// Haze: a surface fades into the sky's horizon colour from where the haze
// starts to the far plane, by its distance from the eye, in flat steps like
// the sky's bands; the last step is the horizon colour itself, so the end
// of the geometry never shows against the sky. The sky (nothing drawn) is
// left as it is.
vec3 distanceHaze(ivec2 p, vec3 col) {
    if (texelFetch(uDepth, p, 0).r >= 1.0) return col;
    const float kSteps = 8.0;
    float dist = length(rayAt(p)) * viewDepth(p);
    float t = clamp((dist - uHazeSpan.x) / (uHazeSpan.y - uHazeSpan.x), 0.0, 1.0);
    return mix(col, uHaze, min(floor(t * kSteps) / (kSteps - 1.0), 1.0));
}

void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec3 col = texelFetch(uColor, p, 0).rgb;
    vec4 np = texelFetch(uNormal, p, 0);
    int surface = int(np.a * kSurfaceScale + 0.5) - 1;
    // An actor's creased material is stone to everything but its crease ink.
    bool actorCrease = surface == kSurfActorCrease;
    if (actorCrease) surface = kSurfStone;
    col = edges(p, col, np, surface, actorCrease);
    col = distanceHaze(p, col);
    oColor = vec4(col, 1.0);
}
