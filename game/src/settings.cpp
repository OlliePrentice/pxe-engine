#include "settings.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
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

// A flash's brightness `age` seconds after it begins, after the video: on
// at full in one frame and held a moment, then fading away over most of a
// second. A flickering flash dips and comes back once before it falls away.
// The fade is smooth here; the toon ramp still lightens its shadows in
// steps, a quarter band at a time.
float flashLevel(float age, bool flicker) {
  if (age < 0.07f) return 1.f;
  if (flicker) {
    if (age < 0.13f) return 0.35f;
    if (age < 0.2f) return 1.f;
    age -= 0.13f;
  }
  const float level = 0.9f * std::exp(-(age - 0.07f) / 0.3f);
  return level < 0.01f ? 0.f : level;
}

}  // namespace

float WorldClock::random() {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return static_cast<float>(rng_ & 0xffffff) / 16777216.f;
}

void WorldClock::strike() {
  // Anywhere around the sky, between 30 and 65 degrees up.
  const float az = random() * 6.2831853f;
  const float el = (30.f + 35.f * random()) * 3.14159265f / 180.f;
  flashDir = {std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az)};
  strikeAge_ = 0.f;
  ++strikes;
  flicker_ = random() < 0.4f;
}

void WorldClock::advance(float dt) {
  // Lightning keeps real time, like the rain, even with the clock paused.
  // It strikes only in rain and downpour, more often the heavier the rain.
  if (lightning && rain >= 0.4f) {
    nextStrike_ -= dt;
    if (nextStrike_ <= 0.f) {
      strike();
      const float heavy = std::clamp((rain - 0.4f) / 0.6f, 0.f, 1.f);
      nextStrike_ = (16.f + (6.f - 16.f) * heavy) * (0.5f + random());
    }
  }
  flash = 0.f;
  if (strikeAge_ >= 0.f) {
    strikeAge_ += dt;
    flash = flashLevel(strikeAge_, flicker_);
    if (flash <= 0.f && strikeAge_ > 0.3f) strikeAge_ = -1.f;
  }

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

void SettingsMenu::draw(WorldClock& clock, ViewToggles& view, SoundSettings& sound, float fps) {
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
      ImGui::SliderFloat("Wind strength", &clock.cloudSpeed, 0.f, 6.f, "%.1fx");
      ImGui::TextDisabled("Moves the clouds and leans the rain");

      ImGui::SeparatorText("Rain");
      ImGui::SliderFloat("Severity", &clock.rain, 0.f, 1.f, "%.2f");
      const struct {
        const char* label;
        float rain;
      } weather[] = {{"None", 0.f}, {"Drizzle", 0.2f}, {"Rain", 0.55f}, {"Downpour", 1.f}};
      for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine();
        if (ImGui::Button(weather[i].label)) clock.rain = weather[i].rain;
      }
      ImGui::TextDisabled("R cycles none / drizzle / rain / downpour");
      ImGui::Checkbox("Lightning", &clock.lightning);
      ImGui::SameLine();
      if (ImGui::Button("Strike now")) clock.strike();
      ImGui::TextDisabled("Flashes in rain and downpour; L strikes now");

      ImGui::SeparatorText("Sound");
      ImGui::Checkbox("Sound on  (M)", &sound.on);
      ImGui::SliderFloat("Volume", &sound.master, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Wind", &sound.wind, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Rain##sound", &sound.rain, 0.f, 1.f, "%.2f");
      ImGui::SliderFloat("Thunder", &sound.thunder, 0.f, 1.f, "%.2f");

      ImGui::SeparatorText("Rendering");
      ImGui::Checkbox("Grass and leaves  (F2)", &view.grass);
      ImGui::SameLine();
      ImGui::Checkbox("Stone edges", &view.edges);
      ImGui::SameLine();
      ImGui::Checkbox("Campfire", &view.fire);
      ImGui::SameLine();
      ImGui::Checkbox("God rays", &view.rays);
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
