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

void clockText(float hour, char* out, int size) {
  const int minutes = static_cast<int>(std::floor(hour * 60.f)) % (24 * 60);
  std::snprintf(out, static_cast<std::size_t>(size), "%02d:%02d", minutes / 60, minutes % 60);
}

}  // namespace

void Clock::advance(float dt) {
  if (paused) return;
  hour = std::fmod(hour + dt / 60.f * hoursPerMinute, 24.f);
  if (hour < 0.f) hour += 24.f;
  // The noise is sampled at position + offset, so moving the offset against
  // the wind carries the clouds with it.
  cloudOffset -= windDir() * kCloudDrift * dt * windStrength;
}

glm::vec2 Clock::windDir() const {
  const float a = windDegrees * 3.14159265f / 180.f;
  return {std::sin(a), -std::cos(a)};
}

pxe::Weather Clock::weather() const {
  pxe::Weather w;
  w.cloudCover = cloudCover;
  w.cloudOffset = cloudOffset;
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

void SettingsMenu::draw(Clock& clock, PlayerSettings& player, ViewSettings& view, float fps) {
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
      ImGui::TextDisabled("Haze starts at 60%% of it");

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
