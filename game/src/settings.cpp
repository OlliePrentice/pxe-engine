#include "settings.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cmath>
#include <cstdio>

namespace game {
namespace {

// Cloud drift in cloud-noise units per real second at speed 1: about a metre
// and a quarter a second over the ground.
constexpr float kCloudDrift = 0.05f;

void clockText(float hour, char* out, int size) {
  const int minutes = static_cast<int>(std::floor(hour * 60.f)) % (24 * 60);
  std::snprintf(out, static_cast<std::size_t>(size), "%02d:%02d", minutes / 60, minutes % 60);
}

}  // namespace

void WorldClock::advance(float dt) {
  if (paused) return;
  hour = std::fmod(hour + dt / 60.f * hoursPerMinute, 24.f);
  if (hour < 0.f) hour += 24.f;
  // The noise is sampled at position + offset, so moving the offset against
  // the wind carries the clouds with it.
  cloudOffset -= windDir() * kCloudDrift * dt * cloudSpeed;
}

glm::vec2 WorldClock::windDir() const {
  const float a = windDegrees * 3.14159265f / 180.f;
  return {std::sin(a), -std::cos(a)};
}

void SettingsMenu::init(GLFWwindow* window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.FontGlobalScale = 1.2f;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 6.f;
  style.FrameRounding = 4.f;
  style.Colors[ImGuiCol_WindowBg].w = 0.92f;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 410");
  ready_ = true;
}

void SettingsMenu::shutdown() {
  if (!ready_) return;
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  ready_ = false;
}

bool SettingsMenu::wantsKeyboard() const { return ready_ && open_ && ImGui::GetIO().WantTextInput; }

void SettingsMenu::draw(WorldClock& clock, ViewToggles& view, float fps) {
  if (!ready_) return;
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

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
      ImGui::Checkbox("Paused  (Space)", &clock.paused);
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
      ImGui::TextDisabled("[ and ] halve or double the speed");

      ImGui::SeparatorText("Clouds and wind");
      ImGui::SliderFloat("Cover", &clock.cloudCover, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Shadow strength", &clock.cloudStrength, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Wind direction", &clock.windDegrees, 0.f, 360.f, "%.0f deg");
      ImGui::SliderFloat("Drift speed", &clock.cloudSpeed, 0.f, 6.f, "%.1fx");

      ImGui::SeparatorText("Rendering");
      ImGui::Checkbox("Grass and leaves  (F2)", &view.grass);
      ImGui::SameLine();
      ImGui::Checkbox("Stone edges", &view.edges);
      if (fps > 0.f)
        ImGui::TextDisabled("%.0f fps (%.1f ms), capped by vsync; --bench measures uncapped", fps, 1000.f / fps);
      else
        ImGui::TextDisabled("measuring fps...");
    }
    ImGui::End();
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace game
