#pragma once

// Private to the renderer's three source files (renderer.cpp,
// renderer_orthographic.cpp, renderer_perspective.cpp), like shader.hpp and
// gl.hpp: the helpers and constants more than one of them uses. Functions
// are defined in renderer.cpp. A helper only one file uses lives in that
// file instead.

#include "pxe/renderer.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace pxe::detail {

constexpr int kFireShadow = 512;  // per cube face

int uloc(unsigned prog, const char* name);

void u1i(unsigned prog, const char* name, int v);
void u1f(unsigned prog, const char* name, float v);
void u2f(unsigned prog, const char* name, float x, float y);
void u3f(unsigned prog, const char* name, float x, float y, float z);
void u3fv(unsigned prog, const char* name, int count, const float* v);
void umat(unsigned prog, const char* name, const glm::mat4& m);

// The characters, each skinned in the vertex shader by its own joints, in
// the program in use (mesh.vert or shadow.vert); one draw each.
void drawActors(unsigned prog, const std::vector<ActorDraw>& actors);

struct LightFit {
  glm::mat4 viewProj{1.f};
  float texel = 0.05f;
};

// Flatten one Palette field across all surfaces into shadow/mid/lit
// triples, or pure red/green/blue per band for --test-colours.
std::vector<float> ramp(const Lighting& lighting, glm::vec3 (Palette::*field)[3], bool testColours);

// The scene's dark regions (Scene::darkRegions), in the toon shaders or the
// edge pass.
void bindDarkRegions(unsigned prog, const Scene& scene);

// This frame's firelights, flattened for the toon shaders.
struct FireUniforms {
  int count = 0;
  std::vector<float> pos;       // xyz each
  std::vector<float> strength;  // with the flicker
  std::vector<int> cube;        // layer / 6 in the cube array, -1 none
  std::vector<float> shadowed;  // 0 no shadows .. 1 full
};

// The firelights' shadow cubes are on unit 7.
void bindToon(unsigned prog, const Lighting& lighting, const Scene& scene, const glm::mat4& lightVP,
              const glm::mat4& flashVP, const FireUniforms& fire, float normalBias, bool testColours);

}  // namespace pxe::detail
