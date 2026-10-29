#include "pxe/renderer.hpp"

#include "pxe/config.hpp"
#include "renderer_internal.hpp"
#include "gl.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace pxe {
using namespace detail;
namespace {

// Under a perspective camera: how far round the eye the sun's map reaches,
// metres. Nothing beyond it is shadowed.
constexpr float kShadowReach = 60.f;

// A view model is drawn into [0, kViewModelDepth] of the depth range, in
// front of every surface of the scene: a surface's depth reaches
// kViewModelDepth only about a millimetre past the near plane.
constexpr double kViewModelDepth = 0.01;

// The same fixed-size square for a perspective camera, kShadowReach round
// the eye pushed half that ahead along the view's flat forward (not at all
// looking straight down), so most of the map lies where the view looks.
// The centre is snapped to the map's texels in the light's own plane, so
// shadow edges hold still as the eye moves rather than crawling. The map
// reaches back toward the light past everything in the scene, so anything
// standing between the light and the square still casts into it.
LightFit fitLightAhead(const Scene& scene, glm::vec3 lightDir, const PerspectiveFrame& camera) {
  LightFit fit;
  const glm::vec3 L = glm::normalize(lightDir);
  const float radius = kShadowReach;
  const glm::vec3 up = std::fabs(L.y) > 0.95f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f};
  fit.texel = 2.f * radius / static_cast<float>(RenderConfig::kShadow);

  glm::vec3 centre = camera.eye + glm::vec3{camera.forward.x, 0.f, camera.forward.z} * (0.5f * radius);
  const glm::mat4 basis = glm::lookAt(glm::vec3{0.f}, -L, up);
  glm::vec4 local = basis * glm::vec4(centre, 1.f);
  local.x = std::round(local.x / fit.texel) * fit.texel;
  local.y = std::round(local.y / fit.texel) * fit.texel;
  centre = glm::vec3(glm::inverse(basis) * local);

  const glm::vec3 sceneCentre = (scene.boundsMin + scene.boundsMax) * 0.5f;
  const float sceneRadius = 0.5f * glm::length(scene.boundsMax - scene.boundsMin) + 1.f;
  const float back = radius + glm::length(centre - sceneCentre) + sceneRadius + 10.f;
  const glm::mat4 view = glm::lookAt(centre + L * back, centre, up);
  fit.viewProj = glm::ortho(-radius, radius, -radius, radius, 1.f, back + radius + 10.f) * view;
  return fit;
}

// The sky's colours greyed under rain cloud, as a room's window panes are.
glm::vec3 overcastSky(glm::vec3 sky, float overcast) {
  return glm::mix(sky, glm::vec3{0.85f * glm::dot(sky, glm::vec3{0.299f, 0.587f, 0.114f})}, overcast);
}
}  // namespace

void Renderer::render(const Scene& scene, const PerspectiveFrame& camera, const FrameParams& params) {
  if (mode_ != RenderMode::kPerspective)
    throw std::invalid_argument(
        "Renderer::render: a PerspectiveFrame needs RenderMode::kPerspective, but init() chose kOrthographic");
  if (params.present && (params.framebufferW <= 0 || params.framebufferH <= 0))
    throw std::invalid_argument("Renderer::render: present is set but FrameParams::framebufferW/H is " +
                                std::to_string(params.framebufferW) + "x" + std::to_string(params.framebufferH) +
                                "; set it to Window::framebufferSize(), or clear present");
  // Indoors there is no sky in view: no cloud shadows and no god rays.
  Lighting lighting = params.lighting;
  if (scene.indoors) {
    lighting.cloudStrength = 0.f;
    lighting.rays = 0.f;
  }
  const LightFit light = fitLightAhead(scene, lighting.lightDir, camera);
  const LightFit flash = fitLightAhead(scene, lighting.flashDir, camera);
  const float normalBias = 3.f * light.texel;
  const glm::mat4 viewProj = camera.proj * camera.view;

  glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  // Both faces: the ground is an open sheet. A slope-scaled offset keeps
  // faces that turn away from the light from shadowing themselves in
  // speckles.
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.f, 4.f);
  beginPass(PassTimes::kShadow);
  // Characters move every frame: with any about, the scene's shadows are
  // drawn into their own map when they change, copied in, and the
  // characters drawn over them. The map is redrawn when the light changes
  // or its fit moves with the eye.
  const bool actors = !params.actors.empty();
  const bool sceneShadows = !shadowValid_ || shadowLight_ != lighting.lightDir || shadowFit_ != light.viewProj;
  if (sceneShadows) {
    glBindFramebuffer(GL_FRAMEBUFFER, actors ? fboShadowStatic_ : fboShadow_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progShadow_);
    umat(progShadow_, "uLightViewProj", light.viewProj);
    for (const Mesh& m : scene.meshes) m.draw();
    for (const Mesh& m : scene.shadowOnly) m.draw();
    shadowValid_ = true;
    shadowLight_ = lighting.lightDir;
    shadowFit_ = light.viewProj;
    shadowStaticValid_ = actors;
  }
  if (actors || shadowHadActors_) {
    if (!shadowStaticValid_) {
      // The scene's map is in shadow_ alone (drawn before any character came): keep a copy.
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fboShadow_);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboShadowStatic_);
      glBlitFramebuffer(0, 0, RenderConfig::kShadow, RenderConfig::kShadow, 0, 0, RenderConfig::kShadow,
                        RenderConfig::kShadow, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
      shadowStaticValid_ = true;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboShadowStatic_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboShadow_);
    glBlitFramebuffer(0, 0, RenderConfig::kShadow, RenderConfig::kShadow, 0, 0, RenderConfig::kShadow,
                      RenderConfig::kShadow, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glUseProgram(progShadow_);
    umat(progShadow_, "uLightViewProj", light.viewProj);
    drawActors(progShadow_, params.actors);
    shadowHadActors_ = actors;
  }
  // Lightning's map: drawn once when a flash strikes from a new direction.
  if (lighting.flash > 0.f && (!flashValid_ || flashLight_ != lighting.flashDir || flashFit_ != flash.viewProj)) {
    glBindFramebuffer(GL_FRAMEBUFFER, fboFlash_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progShadow_);
    umat(progShadow_, "uLightViewProj", flash.viewProj);
    for (const Mesh& m : scene.meshes) m.draw();
    for (const Mesh& m : scene.shadowOnly) m.draw();
    flashValid_ = true;
    flashLight_ = lighting.flashDir;
    flashFit_ = flash.viewProj;
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  endPass();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // No grass, water, fire or grass fire here, so no wind phase, no firelights
  // and no fire grid: the mesh programs without firelights, and the
  // perspective edge pass.
  progEdge_ = edgePerspective_;
  progMesh_ = toon_[0][0].mesh;
  const FireUniforms fireLight;

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, rtW(), rtH());
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadow_);
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_2D, flashShadow_);
  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, fireCubes_);
  const glm::mat4 identity{1.f};

  // Program state shared by every mesh draw this frame.
  glUseProgram(progMesh_);
  u1i(progMesh_, "uShowNdl", params.showNdl ? 1 : 0);
  u1f(progMesh_, "uTime", params.time);
  bindToon(progMesh_, lighting, scene, light.viewProj, flash.viewProj, fireLight, normalBias,
           params.testColours);
  if (!params.actors.empty() || !params.actorEffects.empty() || !params.viewModel.empty()) {
    // The stone's ramp now, over its mid colour by day: the light a character stands in.
    const glm::vec3 day = glm::max(scene.palette[kPhaseDay][kSurfStone].band[1], glm::vec3{0.02f});
    std::vector<float> tint, colour;
    std::vector<int> flags;  // per slot: 1 glows, 2 creases (mesh.frag)
    for (int b = 0; b < 3; ++b) {
      const glm::vec3 t = params.testColours ? glm::vec3(b == 2, b == 1, b == 0) : lighting.palette[kSurfStone].band[b] / day;
      tint.insert(tint.end(), {t.x, t.y, t.z});
    }
    for (const ActorMaterial& m : params.actorMaterials) {
      colour.insert(colour.end(), {m.colour.x, m.colour.y, m.colour.z});
      flags.push_back((m.glow ? 1 : 0) | (m.crease ? 2 : 0));
    }
    u3fv(progMesh_, "uActorTint", 3, tint.data());
    u3fv(progMesh_, "uActorColour", kActorMaterials, colour.data());
    glUniform1iv(uloc(progMesh_, "uActorFlags"), kActorMaterials, flags.data());
    const glm::vec3 fire = lighting.fireBand[kSurfStone] / day;
    u3f(progMesh_, "uActorFire", fire.x, fire.y, fire.z);
  }

  // No water to reflect.
  beginPass(PassTimes::kReflection);
  endPass();

  beginPass(PassTimes::kMeshes);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, rtW(), rtH());
  drawMeshes(scene, params, viewProj, identity, -1e9f);
  // The sky wherever nothing was drawn. Before the window panes, which go
  // over it. The normal target is left as the background's.
  {
    const glm::vec3 horizon = overcastSky(lighting.skyHorizon, lighting.overcast);
    const glm::vec3 zenith = overcastSky(lighting.skyZenith, lighting.overcast);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glUseProgram(progBackdrop_);
    u2f(progBackdrop_, "uTarget", static_cast<float>(rtW()), static_cast<float>(rtH()));
    u3f(progBackdrop_, "uFwd", camera.forward.x, camera.forward.y, camera.forward.z);
    u3f(progBackdrop_, "uRight", camera.right.x, camera.right.y, camera.right.z);
    u3f(progBackdrop_, "uUp", camera.up.x, camera.up.y, camera.up.z);
    u2f(progBackdrop_, "uTanHalf", 1.f / camera.proj[0][0], 1.f / camera.proj[1][1]);
    u3f(progBackdrop_, "uHorizon", horizon.x, horizon.y, horizon.z);
    u3f(progBackdrop_, "uZenith", zenith.x, zenith.y, zenith.z);
    glBindVertexArray(presentVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
  }
  // Indoors, the sky through the windows, where the openings show only the
  // empty background. The normal target is left as the background's.
  if (scene.indoors && skyVertices_ > 0) {
    // Greyed under rain cloud, as the overcast greys the room.
    const glm::vec3 sky = glm::mix(lighting.skyColour,
                                   glm::vec3{0.85f * glm::dot(lighting.skyColour, glm::vec3{0.299f, 0.587f, 0.114f})},
                                   lighting.overcast);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glUseProgram(progSky_);
    umat(progSky_, "uViewProj", viewProj);
    u3f(progSky_, "uSky", sky.x, sky.y, sky.z);
    glBindVertexArray(skyVao_);
    glDrawArrays(GL_TRIANGLES, 0, skyVertices_);
    glBindVertexArray(0);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
  }
  // The view model, in front of everything already drawn.
  if (!params.viewModel.empty()) {
    glDepthRange(0.0, kViewModelDepth);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(progMesh_);
    umat(progMesh_, "uViewProj", viewProj);
    umat(progMesh_, "uMirror", identity);
    u1f(progMesh_, "uClipBelow", -1e9f);
    drawActors(progMesh_, params.viewModel);
    glDepthRange(0.0, 1.0);
  }
  endPass();

  // No water and no sprites. Every pass still begins and ends, so a profiling
  // readback finds every query.
  beginPass(PassTimes::kWater);
  endPass();
  beginPass(PassTimes::kSprites);
  endPass();

  // Edge pass into final_, which is what gets presented and captured.
  beginPass(PassTimes::kEdge);
  glBindFramebuffer(GL_FRAMEBUFFER, fboFinal_);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(progEdge_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, color_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normals_);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, depth_);
  u1i(progEdge_, "uColor", 0);
  u1i(progEdge_, "uNormal", 1);
  u1i(progEdge_, "uDepth", 2);
  u1i(progEdge_, "uEnabled", params.edges ? 1 : 0);
  u3f(progEdge_, "uRim", lighting.rim.x, lighting.rim.y, lighting.rim.z);
  const glm::vec3 edgeLight = glm::normalize(lighting.lightDir);
  u3f(progEdge_, "uLight", edgeLight.x, edgeLight.y, edgeLight.z);
  u3f(progEdge_, "uInk", lighting.ink.x, lighting.ink.y, lighting.ink.z);
  // Light rims stay out of the dark regions.
  bindDarkRegions(progEdge_, scene);
  u1i(progEdge_, "uIndoors", scene.indoors ? 1 : 0);
  u3f(progEdge_, "uEye", camera.eye.x, camera.eye.y, camera.eye.z);
  u3f(progEdge_, "uRight", camera.right.x, camera.right.y, camera.right.z);
  u3f(progEdge_, "uUp", camera.up.x, camera.up.y, camera.up.z);
  u3f(progEdge_, "uFwd", camera.forward.x, camera.forward.y, camera.forward.z);
  u1f(progEdge_, "uNear", camera.nearPlane);
  // The view model's pixels are those drawn into [0, kViewModelDepth] of
  // the depth range; with none in the frame, no pixel is.
  u1f(progEdge_, "uViewModelDepth", params.viewModel.empty() ? 0.f : static_cast<float>(kViewModelDepth));
  // Rebuilding positions from the perspective depth, the depth steps that
  // count as outlines, and the haze.
  {
    const float tanY = 1.f / camera.proj[1][1];
    u1f(progEdge_, "uFar", camera.farPlane);
    u2f(progEdge_, "uTanHalf", 1.f / camera.proj[0][0], tanY);
    u1f(progEdge_, "uPixelAngle", 2.f * tanY / static_cast<float>(rtH()));
    u1f(progEdge_, "uRefTexel", 1.f / RenderConfig::kPixelsPerUnit);
    const glm::vec3 haze = overcastSky(lighting.skyHorizon, lighting.overcast);
    u3f(progEdge_, "uHaze", haze.x, haze.y, haze.z);
    u2f(progEdge_, "uHazeSpan", 0.6f * camera.farPlane, camera.farPlane);
  }
  glBindVertexArray(presentVao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  glEnable(GL_DEPTH_TEST);

  endPass();

  beginPass(PassTimes::kPresent);
  if (params.present) drawPresent(glm::vec2{0.f}, params);
  endPass();
  if (profiling_) {
    for (int i = 0; i < PassTimes::kCount; ++i) {
      GLuint64 ns = 0;
      glGetQueryObjectui64v(queries_[i], GL_QUERY_RESULT, &ns);
      passTimes_.ms[i] = static_cast<double>(ns) * 1e-6;
    }
  }
  pxeGlCheck("render");
}

}  // namespace pxe
