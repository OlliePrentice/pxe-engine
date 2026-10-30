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

// Metres a second the dust drifts at wind strength 1.
constexpr float kDustSpeed = 2.f;

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
  fit.depthSpan = (back + radius + 10.f) - 1.f;
  return fit;
}

// A display (sRGB) colour in linear light, by the exact sRGB curve per
// channel. (lighting.cpp keeps its own copy.)
glm::vec3 toLinear(glm::vec3 c) {
  auto one = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
  return glm::vec3{one(c.x), one(c.y), one(c.z)};
}

// Everything the smooth lit mesh program (mesh_perspective.frag) reads: the
// light and its shadows, the albedo of every surface and character
// material. The sun's shadow map is on unit 1 and lightning's on unit 6.
// With `testColours` every albedo is white, the sun light pure red, the sky
// fill pure green and the bounce pure blue, so each term shows on its own.
void bindLit(unsigned prog, const Lighting& lighting, const Scene& scene, const FrameParams& params,
             const LightFit& light, const LightFit& flash, float normalBias, glm::vec3 eye, bool testColours) {
  const glm::vec3 sunDir = glm::normalize(lighting.lightDir);
  u3f(prog, "uSunDir", sunDir.x, sunDir.y, sunDir.z);
  const glm::vec3 sun = testColours ? glm::vec3{1.f, 0.f, 0.f} : lighting.sunLight;
  const glm::vec3 fill = testColours ? glm::vec3{0.f, 1.f, 0.f} : lighting.skyFillLight;
  const glm::vec3 bounce = testColours ? glm::vec3{0.f, 0.f, 1.f} : lighting.bounceLight;
  u3f(prog, "uSunLight", sun.x, sun.y, sun.z);
  u3f(prog, "uSkyFillLight", fill.x, fill.y, fill.z);
  u3f(prog, "uBounceLight", bounce.x, bounce.y, bounce.z);
  u1f(prog, "uExposure", lighting.exposure);

  std::vector<float> albedo;
  albedo.reserve(kSurfCount * 3);
  for (int s = 0; s < kSurfCount; ++s) {
    const glm::vec3 c = testColours ? glm::vec3{1.f} : toLinear(scene.albedo[s]);
    albedo.insert(albedo.end(), {c.x, c.y, c.z});
  }
  u3fv(prog, "uAlbedo", kSurfCount, albedo.data());
  std::vector<float> colour, actorAlbedo;
  std::vector<int> flags;  // per slot: 1 glows, 2 creases (mesh_perspective.frag)
  for (const ActorMaterial& m : params.actorMaterials) {
    const glm::vec3 c = testColours ? glm::vec3{1.f} : toLinear(m.colour);
    colour.insert(colour.end(), {m.colour.x, m.colour.y, m.colour.z});
    actorAlbedo.insert(actorAlbedo.end(), {c.x, c.y, c.z});
    flags.push_back((m.glow ? 1 : 0) | (m.crease ? 2 : 0));
  }
  u3fv(prog, "uActorColour", kActorMaterials, colour.data());
  u3fv(prog, "uActorAlbedo", kActorMaterials, actorAlbedo.data());
  glUniform1iv(uloc(prog, "uActorFlags"), kActorMaterials, flags.data());

  umat(prog, "uLightViewProj", light.viewProj);
  u1i(prog, "uShadowMap", 1);
  u1f(prog, "uNormalBias", normalBias);
  // Half the penumbra, in the map's UV (the map spans 2 * kShadowReach
  // metres) and in its depth units. The flash map shares the fit's size.
  u1f(prog, "uPenumbraUV", lighting.shadowPenumbra * 0.5f / (2.f * kShadowReach));
  u1f(prog, "uPenumbraDepth", lighting.shadowPenumbra * 0.5f / light.depthSpan);
  glUniform4f(uloc(prog, "uHorizon"), lighting.horizon.x, lighting.horizon.y, lighting.horizon.z, lighting.horizon.w);
  u1f(prog, "uHorizonRelief", lighting.horizonRelief);
  u1f(prog, "uCloudCover", lighting.cloudCover);
  u2f(prog, "uCloudOffset", lighting.cloudOffset.x, lighting.cloudOffset.y);
  u1f(prog, "uCloudStrength", lighting.cloudStrength);
  u1f(prog, "uOvercast", lighting.overcast);
  u1f(prog, "uRain", lighting.rain);
  u1f(prog, "uFlash", lighting.flash);
  const glm::vec3 flashDir = glm::normalize(lighting.flashDir);
  u3f(prog, "uFlashDir", flashDir.x, flashDir.y, flashDir.z);
  umat(prog, "uFlashViewProj", flash.viewProj);
  u1i(prog, "uFlashShadowMap", 6);

  // Surface shading: each surface's terminator sharpness, and the ripples.
  // Test colours show the plain light terms, so neither applies.
  std::vector<float> terminator(kSurfCount, 0.f);
  if (!testColours) std::copy(scene.terminator, scene.terminator + kSurfCount, terminator.begin());
  glUniform1fv(uloc(prog, "uTerminator"), kSurfCount, terminator.data());
  const SurfaceRipples& r = scene.ripples;
  const float heading = glm::radians(r.headingDegrees);
  glUniform1i(uloc(prog, "uRippleSurface"), testColours ? -1 : r.surface);
  u1f(prog, "uRippleWavelength", r.wavelength);
  u1f(prog, "uRippleDepth", r.depth);
  u1f(prog, "uRippleFadeStart", r.fadeStart);
  u1f(prog, "uRippleFadeEnd", r.fadeEnd);
  u1f(prog, "uRippleMaxSlope", glm::radians(r.maxSlopeDegrees));
  u2f(prog, "uRippleWind", std::sin(heading), std::cos(heading));
  u3f(prog, "uEye", eye.x, eye.y, eye.z);
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
  // Toward the sun for the sky's glow and disc and the haze's glow, which
  // rain cloud fades.
  const glm::vec3 sunDir = glm::normalize(lighting.sunDir);
  const float sunGlow = lighting.sunGlow * (1.f - lighting.overcast);
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
  // and no fire grid: the smooth lit mesh program, and the perspective edge
  // pass.
  progEdge_ = edgePerspective_;
  progMesh_ = progMeshLit_;

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
  bindLit(progMesh_, lighting, scene, params, light, flash, normalBias, camera.eye, params.testColours);

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
    u3f(progBackdrop_, "uSunDir", sunDir.x, sunDir.y, sunDir.z);
    u3f(progBackdrop_, "uSunColour", lighting.sunGlowColour.x, lighting.sunGlowColour.y, lighting.sunGlowColour.z);
    u1f(progBackdrop_, "uSunGlow", sunGlow);
    u1f(progBackdrop_, "uOvercastDisc", lighting.overcast);
    // The sky clouds: the wind they stream along, and each layer's plane.
    u3f(progBackdrop_, "uEye", camera.eye.x, camera.eye.y, camera.eye.z);
    glm::vec2 windDir = lighting.windDir;
    windDir = glm::length(windDir) < 1e-4f ? glm::vec2{1.f, 0.f} : glm::normalize(windDir);
    u2f(progBackdrop_, "uWindDir", windDir.x, windDir.y);
    float cloudHeight[Scene::kSkyCloudLayers], cloudCover[Scene::kSkyCloudLayers];
    float cloudOpacity[Scene::kSkyCloudLayers], cloudLength[Scene::kSkyCloudLayers];
    float cloudWidth[Scene::kSkyCloudLayers];
    glm::vec2 cloudOffset[Scene::kSkyCloudLayers];
    for (int i = 0; i < Scene::kSkyCloudLayers; ++i) {
      const SkyCloudLayer& layer = scene.skyClouds[i];
      cloudHeight[i] = layer.height;
      cloudCover[i] = layer.cover;
      cloudOpacity[i] = layer.opacity;
      cloudLength[i] = layer.length;
      cloudWidth[i] = layer.width;
      cloudOffset[i] = lighting.skyCloudOffset * layer.drift;
    }
    glUniform1fv(uloc(progBackdrop_, "uCloudHeight"), Scene::kSkyCloudLayers, cloudHeight);
    glUniform1fv(uloc(progBackdrop_, "uCloudCover"), Scene::kSkyCloudLayers, cloudCover);
    glUniform1fv(uloc(progBackdrop_, "uCloudOpacity"), Scene::kSkyCloudLayers, cloudOpacity);
    glUniform1fv(uloc(progBackdrop_, "uCloudLength"), Scene::kSkyCloudLayers, cloudLength);
    glUniform1fv(uloc(progBackdrop_, "uCloudWidth"), Scene::kSkyCloudLayers, cloudWidth);
    glUniform2fv(uloc(progBackdrop_, "uCloudOffset"), Scene::kSkyCloudLayers, &cloudOffset[0].x);
    u1f(progBackdrop_, "uCloudBrightness", scene.skyCloudBrightness);
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
  // Airborne dust, over the scene and sky, behind the view model. The wind's
  // carry advances every frame, with or without dust, so turning the dust on
  // doesn't make it jump.
  {
    const float dt = dustTime_ < 0.f ? 0.f : std::clamp(params.time - dustTime_, 0.f, 0.25f);
    dustTime_ = params.time;
    const glm::vec2 wind = glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{1.f, 0.f};
    dustOffset_ += glm::vec3{wind.x, 0.f, wind.y} * (kDustSpeed * lighting.windStrength * dt);
    const int count = static_cast<int>(std::lround(std::clamp(scene.dustDensity, 0.f, 1.f) * kDustMax));
    if (count > 0) {
      const glm::vec3 haze = overcastSky(lighting.skyHorizon, lighting.overcast);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthFunc(GL_LEQUAL);
      glDepthMask(GL_FALSE);
      glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glUseProgram(progDust_);
      umat(progDust_, "uViewProj", viewProj);
      u3f(progDust_, "uEye", camera.eye.x, camera.eye.y, camera.eye.z);
      u3f(progDust_, "uOffset", dustOffset_.x, dustOffset_.y, dustOffset_.z);
      u1f(progDust_, "uTime", params.time);
      u1f(progDust_, "uSize", std::clamp(scene.dustSize, 1.f, 2.f));
      u3f(progDust_, "uHaze", haze.x, haze.y, haze.z);
      u3f(progDust_, "uSunDir", sunDir.x, sunDir.y, sunDir.z);
      u3f(progDust_, "uSunColour", lighting.sunGlowColour.x, lighting.sunGlowColour.y, lighting.sunGlowColour.z);
      u1f(progDust_, "uSunGlow", sunGlow);
      glBindVertexArray(dustVao_);
      glDrawArrays(GL_POINTS, 0, count);
      glBindVertexArray(0);
      glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDepthMask(GL_TRUE);
      glDepthFunc(GL_LESS);
      glDisable(GL_BLEND);
    }
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
    u1f(progEdge_, "uHazeDensity", lighting.hazeDensity);
    u3f(progEdge_, "uSunDir", sunDir.x, sunDir.y, sunDir.z);
    u3f(progEdge_, "uSunColour", lighting.sunGlowColour.x, lighting.sunGlowColour.y, lighting.sunGlowColour.z);
    u1f(progEdge_, "uSunGlow", sunGlow);
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
