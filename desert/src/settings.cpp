#include "settings.hpp"

#include "pxe/overlay.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace desert {
namespace {

// Cloud drift in cloud-noise units per real second at speed 1: about a metre
// and a quarter a second over the ground.
constexpr float kCloudDrift = 0.05f;

constexpr float kSkyCloudDrift = 15.f;  // metres a second the sky clouds drift at wind strength 1

void clockText(float hour, char* out, int size) {
  const int minutes = static_cast<int>(std::floor(hour * 60.f)) % (24 * 60);
  std::snprintf(out, static_cast<std::size_t>(size), "%02d:%02d", minutes / 60, minutes % 60);
}

// Writes the scene's light and sky as C++ statements for desert::buildWorld,
// all four phases, the four single values, then the sky clouds and dust, then
// the ground (the sand's albedo and terminator and the ripples), and flushes.
void printLighting(const pxe::Scene& scene) {
  static const char* const kPhaseNames[pxe::kPhaseCount] = {"kPhaseDay", "kPhaseGolden", "kPhaseTwilight",
                                                            "kPhaseNight"};
  std::printf("// pxe_desert lighting: paste into desert::buildWorld\n");
  for (int i = 0; i < pxe::kPhaseCount; ++i) {
    const char* name = kPhaseNames[i];
    const auto colour = [&](const char* field, const glm::vec3& c) {
      std::printf("scene.%s[pxe::%s] = {%.3ff, %.3ff, %.3ff};\n", field, name, c.x, c.y, c.z);
    };
    const auto strength = [&](const char* field, float v) {
      std::printf("scene.%s[pxe::%s] = %.2ff;\n", field, name, v);
    };
    colour("sunColour", scene.sunColour[i]);
    strength("sunStrength", scene.sunStrength[i]);
    colour("skyFill", scene.skyFill[i]);
    strength("skyFillStrength", scene.skyFillStrength[i]);
    colour("groundBounce", scene.groundBounce[i]);
    strength("groundBounceStrength", scene.groundBounceStrength[i]);
    colour("skyHorizon", scene.skyHorizon[i]);
    colour("skyZenith", scene.skyZenith[i]);
  }
  std::printf("scene.hazeDensity = %.4ff;\n", scene.hazeDensity);
  std::printf("scene.sunGlow = %.2ff;\n", scene.sunGlow);
  std::printf("scene.shadowPenumbra = %.2ff;\n", scene.shadowPenumbra);
  std::printf("scene.exposure = %.2ff;\n", scene.exposure);
  for (int i = 0; i < pxe::Scene::kSkyCloudLayers; ++i) {
    const pxe::SkyCloudLayer& layer = scene.skyClouds[i];
    std::printf("scene.skyClouds[%d].height = %.1ff;\n", i, layer.height);
    std::printf("scene.skyClouds[%d].cover = %.2ff;\n", i, layer.cover);
    std::printf("scene.skyClouds[%d].opacity = %.2ff;\n", i, layer.opacity);
    std::printf("scene.skyClouds[%d].length = %.1ff;\n", i, layer.length);
  }
  std::printf("scene.skyCloudBrightness = %.2ff;\n", scene.skyCloudBrightness);
  std::printf("scene.dustDensity = %.2ff;\n", scene.dustDensity);
  std::printf("scene.dustSize = %.2ff;\n", scene.dustSize);
  std::printf("scene.albedo[pxe::kSurfSand] = {%.3ff, %.3ff, %.3ff};\n", scene.albedo[pxe::kSurfSand].x,
              scene.albedo[pxe::kSurfSand].y, scene.albedo[pxe::kSurfSand].z);
  std::printf("scene.terminator[pxe::kSurfSand] = %.2ff;\n", scene.terminator[pxe::kSurfSand]);
  const pxe::SurfaceRipples& ripples = scene.ripples;
  if (ripples.surface == pxe::kSurfSand)
    std::printf("scene.ripples.surface = pxe::kSurfSand;\n");
  else
    std::printf("scene.ripples.surface = %d;\n", ripples.surface);
  std::printf("scene.ripples.wavelength = %.3ff;\n", ripples.wavelength);
  std::printf("scene.ripples.depth = %.2ff;\n", ripples.depth);
  std::printf("scene.ripples.headingDegrees = %.1ff;\n", ripples.headingDegrees);
  std::printf("scene.ripples.fadeStart = %.1ff;\n", ripples.fadeStart);
  std::printf("scene.ripples.fadeEnd = %.1ff;\n", ripples.fadeEnd);
  std::printf("scene.ripples.maxSlopeDegrees = %.1ff;\n", ripples.maxSlopeDegrees);
  std::fflush(stdout);
}

}  // namespace

void Clock::advance(float dt) {
  if (paused) return;
  hour = std::fmod(hour + dt / 60.f * hoursPerMinute, 24.f);
  if (hour < 0.f) hour += 24.f;
  // The noise is sampled at position + offset, so moving the offset against
  // the wind carries the clouds with it.
  cloudOffset -= windDir() * kCloudDrift * dt * windStrength;
  skyCloudOffset += windDir() * kSkyCloudDrift * dt * windStrength;
}

glm::vec2 Clock::windDir() const {
  const float a = windDegrees * 3.14159265f / 180.f;
  return {std::sin(a), -std::cos(a)};
}

pxe::Weather Clock::weather() const {
  pxe::Weather w;
  w.cloudCover = cloudCover;
  w.cloudOffset = cloudOffset;
  w.skyCloudOffset = skyCloudOffset;
  w.cloudStrength = cloudStrength;
  w.windDir = windDir();
  w.windStrength = windStrength;
  return w;
}

void SettingsMenu::init(pxe::Overlay& overlay) {
  overlay_ = &overlay;
  ImGui::GetIO().FontGlobalScale = 1.2f;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 6.f;
  style.FrameRounding = 4.f;
  style.Colors[ImGuiCol_WindowBg].w = 0.92f;
}

bool SettingsMenu::wantsKeyboard() const { return overlay_ && open_ && overlay_->wantsKeyboard(); }

void SettingsMenu::draw(Clock& clock, PlayerSettings& player, ViewSettings& view, pxe::Scene& scene,
                        const pxe::Lighting& lighting, float fps) {
  if (!overlay_) return;
  overlay_->begin();

  if (open_) {
    ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400.f, 0.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings  (Tab to close)", &open_, ImGuiWindowFlags_AlwaysAutoResize)) {
      char now[16];
      clockText(clock.hour, now, sizeof(now));
      ImGui::SeparatorText("Time of day");
      ImGui::Text("Clock  %s%s", now, clock.paused ? "   (paused)" : "");
      ImGui::SliderFloat("Hour", &clock.hour, 0.f, 24.f, "%.2f");
      const struct {
        const char* label;
        float hour;
      } presets[] = {{"Dawn", 5.6f}, {"Morning", 8.5f}, {"Noon", 12.f}, {"Golden", 17.6f}, {"Dusk", 18.6f}, {"Night", 22.5f}};
      for (int i = 0; i < 6; ++i) {
        if (i) ImGui::SameLine();
        if (ImGui::Button(presets[i].label)) clock.hour = presets[i].hour;
      }

      ImGui::SeparatorText("Time speed");
      ImGui::Checkbox("Paused", &clock.paused);
      ImGui::SliderFloat("Game hours / real minute", &clock.hoursPerMinute, 0.0167f, 120.f, "%.2f",
                         ImGuiSliderFlags_Logarithmic);
      const float dayMinutes = 24.f / clock.hoursPerMinute;
      if (dayMinutes < 120.f)
        ImGui::TextDisabled("One day lasts %.1f real minutes", dayMinutes);
      else
        ImGui::TextDisabled("One day lasts %.1f real hours", dayMinutes / 60.f);
      const struct {
        const char* label;
        float rate;
      } speeds[] = {{"Real time", 1.f / 60.f}, {"1 min = 1 h", 1.f}, {"Default", 6.f}, {"Fast", 24.f}, {"Very fast", 96.f}};
      for (int i = 0; i < 5; ++i) {
        if (i) ImGui::SameLine();
        if (ImGui::Button(speeds[i].label)) clock.hoursPerMinute = speeds[i].rate;
      }

      ImGui::SeparatorText("Clouds and wind");
      ImGui::SliderFloat("Cover", &clock.cloudCover, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Shadow strength", &clock.cloudStrength, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Wind direction", &clock.windDegrees, 0.f, 360.f, "%.0f deg");
      ImGui::SliderFloat("Wind strength", &clock.windStrength, 0.f, 6.f, "%.1fx");
      ImGui::TextDisabled("Wind drifts the clouds");

      ImGui::SeparatorText("Player");
      ImGui::SliderFloat("Walk speed", &player.walkSpeed, 1.f, 10.f, "%.1f m/s");
      // The sprint stays at least half a metre a second over the walk, which
      // the arms' blend between the two clips needs.
      player.sprintSpeed = std::max(player.sprintSpeed, player.walkSpeed + 0.5f);
      ImGui::SliderFloat("Sprint speed", &player.sprintSpeed, player.walkSpeed + 0.5f, 15.f, "%.1f m/s");
      ImGui::SliderFloat("Field of view", &player.fovDegrees, 40.f, 100.f, "%.0f deg");
      ImGui::SliderFloat("Mouse sensitivity", &player.lookScale, 0.25f, 4.f, "%.2fx", ImGuiSliderFlags_Logarithmic);

      ImGui::SeparatorText("Arms");
      ImGui::Checkbox("Show arms", &view.showArms);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Preview");
      ImGui::SameLine();
      ImGui::RadioButton("Off", &view.armsPreview, ViewSettings::kPreviewOff);
      ImGui::SameLine();
      ImGui::RadioButton("Walk", &view.armsPreview, ViewSettings::kPreviewWalk);
      ImGui::SameLine();
      ImGui::RadioButton("Sprint", &view.armsPreview, ViewSettings::kPreviewSprint);
      ImGui::BeginDisabled(view.armsPreview == ViewSettings::kPreviewOff);
      ImGui::Checkbox("Hold pose", &view.holdPose);
      ImGui::SliderFloat("Stride phase", &view.holdPhase, 0.f, 1.f, "%.2f");
      ImGui::EndDisabled();
      ImGui::TextDisabled("Preview plays a clip while standing still");

      ImGui::SeparatorText("Rendering");
      ImGui::Checkbox("Edges", &view.edges);
      ImGui::SameLine();
      ImGui::Checkbox("N·L", &view.showNdl);
      ImGui::SameLine();
      ImGui::Checkbox("Test colours", &view.testColours);
      ImGui::SliderFloat("View distance", &view.viewDistance, 100.f, 300.f, "%.0f m");
      ImGui::TextDisabled("The haze is full at it");

      ImGui::SeparatorText("Lighting");
      // The phase in effect now: the largest weight, the first index on a tie.
      int ph = 0;
      for (int i = 1; i < pxe::kPhaseCount; ++i)
        if (lighting.phaseWeight[i] > lighting.phaseWeight[ph]) ph = i;
      static const char* const kPhaseLabels[pxe::kPhaseCount] = {"Day", "Golden", "Twilight", "Night"};
      ImGui::Text("Editing: %s", kPhaseLabels[ph]);
      ImGui::TextDisabled("the phase in effect now; change the hour for the others");
      const bool night = ph == pxe::kPhaseNight;
      // At night the direct light is the moon's. ImGui hashes only what
      // follows "###", so the label changes and the widget's ID does not.
      ImGui::ColorEdit3(night ? "Moon###sunColour" : "Sun###sunColour", &scene.sunColour[ph].x);
      ImGui::SliderFloat(night ? "Moon strength###sunStrength" : "Sun strength###sunStrength",
                         &scene.sunStrength[ph], 0.f, 3.f, "%.2f");
      ImGui::ColorEdit3("Sky fill", &scene.skyFill[ph].x);
      ImGui::SliderFloat("Sky fill strength", &scene.skyFillStrength[ph], 0.f, 1.5f, "%.2f");
      ImGui::ColorEdit3("Ground bounce", &scene.groundBounce[ph].x);
      ImGui::SliderFloat("Bounce strength", &scene.groundBounceStrength[ph], 0.f, 1.f, "%.2f");
      ImGui::ColorEdit3("Sky horizon", &scene.skyHorizon[ph].x);
      ImGui::ColorEdit3("Sky zenith", &scene.skyZenith[ph].x);
      ImGui::SliderFloat("Haze density", &scene.hazeDensity, 0.f, 0.03f, "%.4f /m");
      ImGui::SliderFloat("Sun glow", &scene.sunGlow, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Shadow penumbra", &scene.shadowPenumbra, 0.f, 1.f, "%.2f m");
      ImGui::SliderFloat("Exposure", &scene.exposure, 0.25f, 4.f, "%.2fx", ImGuiSliderFlags_Logarithmic);
      if (ImGui::Button("Print values")) printLighting(scene);
      ImGui::TextDisabled("Prints all four phases, the sky and the ground to the terminal as code for world.cpp");

      ImGui::SeparatorText("Sky and dust");
      static const char* const kLayerLabels[pxe::Scene::kSkyCloudLayers] = {"High clouds", "Low clouds"};
      for (int i = 0; i < pxe::Scene::kSkyCloudLayers; ++i) {
        pxe::SkyCloudLayer& layer = scene.skyClouds[i];
        char id[32];
        ImGui::Text("%s", kLayerLabels[i]);
        std::snprintf(id, sizeof(id), "Cover##%d", i);
        ImGui::SliderFloat(id, &layer.cover, 0.f, 1.f, "%.2f");
        std::snprintf(id, sizeof(id), "Opacity##%d", i);
        ImGui::SliderFloat(id, &layer.opacity, 0.f, 1.f, "%.2f");
        std::snprintf(id, sizeof(id), "Altitude##%d", i);
        ImGui::SliderFloat(id, &layer.height, 500.f, 15000.f, "%.0f m", ImGuiSliderFlags_Logarithmic);
        std::snprintf(id, sizeof(id), "Streak length##%d", i);
        ImGui::SliderFloat(id, &layer.length, 500.f, 12000.f, "%.0f m", ImGuiSliderFlags_Logarithmic);
      }
      ImGui::SliderFloat("Cloud brightness", &scene.skyCloudBrightness, 0.f, 2.f, "%.2f");
      ImGui::SliderFloat("Dust density", &scene.dustDensity, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Dust size", &scene.dustSize, 1.f, 2.f, "%.1f px");
      ImGui::TextDisabled("Clouds cast no shadows; ground cloud shadows are Clouds and wind's Cover");

      ImGui::SeparatorText("Ground");
      ImGui::ColorEdit3("Sand", &scene.albedo[pxe::kSurfSand].x);
      ImGui::SliderFloat("Terminator sharpness", &scene.terminator[pxe::kSurfSand], 0.f, 1.f, "%.2f");
      pxe::SurfaceRipples& ripples = scene.ripples;
      ImGui::SliderFloat("Ripple wavelength", &ripples.wavelength, 0.05f, 1.f, "%.2f m", ImGuiSliderFlags_Logarithmic);
      ImGui::SliderFloat("Ripple depth", &ripples.depth, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Ripple heading", &ripples.headingDegrees, 0.f, 360.f, "%.0f deg");
      ImGui::SliderFloat("Ripples full to", &ripples.fadeStart, 0.f, 30.f, "%.1f m");
      // The fade ends at least a metre past where it starts.
      ripples.fadeEnd = std::max(ripples.fadeEnd, ripples.fadeStart + 1.f);
      ImGui::SliderFloat("Ripples gone by", &ripples.fadeEnd, 1.f, 60.f, "%.1f m");
      ripples.fadeStart = std::min(ripples.fadeStart, ripples.fadeEnd - 1.f);
      ImGui::SliderFloat("Ripple slope cutoff", &ripples.maxSlopeDegrees, 0.f, 45.f, "%.0f deg");
      ImGui::TextDisabled("Dune shapes are fixed in world.cpp");

      if (fps > 0.f)
        ImGui::TextDisabled("%.0f fps (%.1f ms)", fps, 1000.f / fps);
      else
        ImGui::TextDisabled("measuring fps...");
    }
    ImGui::End();
  }

  overlay_->end();
}

}  // namespace desert
