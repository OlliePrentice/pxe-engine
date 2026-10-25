#include "settings.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cfloat>
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
  // The campfires: Rain or Downpour puts out each one open to the sky, over
  // about eight seconds in rain and three and a half in a downpour, and it
  // stays out until it is relit, catching again over a couple of seconds.
  // A fire under a roof burns on. A dying fire smokes harder and wisps on
  // for about twenty seconds after the last flame. After the first, each
  // fire takes a little longer to catch or die than the one before (up to
  // about a third longer), so they do not all go in lockstep. A small flame
  // - a candle - catches and dies in about a quarter of the time.
  const float heavy = std::clamp((rain - kRainPutsOutFire) / (1.f - kRainPutsOutFire), 0.f, 1.f);
  for (std::size_t i = 0; i < fires.size(); ++i) {
    Fire& f = fires[i];
    float pace = 1.f + 0.35f * std::fmod(0.618034f * static_cast<float>(i), 1.f);
    if (f.size < 1.f) pace *= 0.25f;
    if (f.lit && !f.sheltered && rain >= kRainPutsOutFire) f.lit = false;
    if (f.doused) {
      // Doused: out in about half a second, in a burst of smoke.
      f.level = std::max(0.f, f.level - dt / 0.5f);
      if (f.level > 0.f) f.smoke = std::max(f.smoke, 1.f - 0.5f * f.level);
      else f.smoke = std::max(0.f, f.smoke - dt / 20.f);
    } else if (f.lit) {
      f.level = std::min(1.f, f.level + dt / (2.5f * pace));
      f.smoke = std::max(0.f, f.smoke - dt / 20.f);
    } else {
      f.level = std::max(0.f, f.level - dt / ((8.f - 4.5f * heavy) * pace));
      if (f.level > 0.f) f.smoke = std::max(f.smoke, 1.f - f.level);
      else f.smoke = std::max(0.f, f.smoke - dt / 20.f);
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

void WorldClock::placeFires(const std::vector<pxe::FireSite>& sites) {
  fires.clear();
  nextFireId = 0;
  for (const pxe::FireSite& site : sites) {
    Fire f;
    f.id = nextFireId++;
    f.base = site.base;
    f.sheltered = site.sheltered;
    f.size = site.size;
    fires.push_back(f);
  }
}

std::uint32_t WorldClock::addFire(glm::vec3 base, const pxe::ShelterMap& shelter, float size) {
  Fire f;
  f.id = nextFireId++;
  f.base = base;
  f.size = size;
  f.sheltered = shelter.sheltered(base);
  f.level = 0.f;  // it catches
  fires.push_back(f);
  return f.id;
}

void WorldClock::removeFire(std::uint32_t id) {
  std::erase_if(fires, [&](const Fire& f) { return f.id == id; });
}

void WorldClock::relightFires() {
  for (Fire& f : fires) {
    f.lit = true;
    f.doused = false;
  }
}

bool WorldClock::underStroke(const Fire& f, glm::vec2 from, glm::vec2 to, float radius) {
  const glm::vec2 along = to - from;
  const float len2 = glm::dot(along, along);
  const glm::vec2 p{f.base.x, f.base.z};
  const float t = len2 > 1e-8f ? std::clamp(glm::dot(p - from, along) / len2, 0.f, 1.f) : 0.f;
  // A campfire's flames spread past its base, so a stroke near it counts.
  return glm::distance(p, from + along * t) <= radius + 0.4f * f.size;
}

void WorldClock::douse(glm::vec2 from, glm::vec2 to, float radius) {
  for (Fire& f : fires) {
    if (!underStroke(f, from, to, radius)) continue;
    f.lit = false;
    f.doused = true;
  }
}

void WorldClock::kindle(glm::vec2 from, glm::vec2 to, float radius) {
  for (Fire& f : fires) {
    if (!underStroke(f, from, to, radius)) continue;
    f.lit = true;
    f.doused = false;
  }
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

bool SettingsMenu::wantsMouse() const { return ready_ && open_ && ImGui::GetIO().WantCaptureMouse; }

void SettingsMenu::draw(WorldClock& clock, ViewToggles& view, SoundSettings& sound, BrushSettings& brush,
                        const std::vector<glm::vec2>& brushOutline, const char* note, glm::vec2 notePoint,
                        float fps, const LoadingScreen* loading) {
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
      ImGui::Checkbox("Fireflies", &view.fireflies);

      ImGui::SeparatorText("Campfire");
      int burning = 0, dying = 0, smoking = 0, sheltered = 0;
      for (const WorldClock::Fire& f : clock.fires) {
        burning += f.lit;
        dying += !f.lit && f.level > 0.f;
        smoking += !f.lit && f.level <= 0.f && f.smoke > 0.f;
        sheltered += f.sheltered;
      }
      const int out = static_cast<int>(clock.fires.size()) - burning;
      if (clock.fires.size() == 1) {
        const WorldClock::Fire& f = clock.fires[0];
        ImGui::Text("%s", f.lit ? (f.level < 1.f ? "Catching" : "Burning")
                          : f.level > 0.f ? "Going out in the rain"
                          : f.smoke > 0.f ? "Out, still smoking" : "Out");
      } else if (dying > 0) {
        ImGui::Text("%d burning, %d going out in the rain", burning, dying);
      } else {
        ImGui::Text("%d burning, %d out%s", burning, out, smoking > 0 ? ", still smoking" : "");
      }
      const bool tooWet = clock.rain >= WorldClock::kRainPutsOutFire;
      ImGui::BeginDisabled(out == 0 || tooWet);
      ImGui::SameLine();
      if (ImGui::Button(out > 1 ? "Relight fires" : "Relight fire")) clock.relightFires();
      ImGui::EndDisabled();
      const char* why = sheltered > 0 ? "Rain puts out fires open to the sky; roofed ones burn on"
                                      : "Rain and Downpour put it out";
      if (tooWet) ImGui::TextDisabled("Too wet to relight. %s", why);
      else ImGui::TextDisabled("%s", why);
      ImGui::SeparatorText("Grass fire");
      {
        // The far end of the slider is "never dies out" (stored as 0).
        constexpr float kForever = 125.f;
        float life = clock.grassFireLife > 0.f ? clock.grassFireLife : kForever;
        if (ImGui::SliderFloat("Spreads for", &life, 5.f, kForever, life >= kForever ? "until all is burnt" : "%.0f s"))
          clock.grassFireLife = life >= kForever ? 0.f : life;
        ImGui::TextDisabled("How long a fire keeps spreading before it dies out; G lights one");
      }

      ImGui::SeparatorText("Spell brushes");
      ImGui::RadioButton("Off", &brush.kind, BrushSettings::kNone);
      ImGui::SameLine();
      ImGui::RadioButton("Fire  (1)", &brush.kind, 0);
      ImGui::SameLine();
      ImGui::RadioButton("Water  (2)", &brush.kind, 1);
      ImGui::SameLine();
      ImGui::RadioButton("Regrow  (3)", &brush.kind, 2);
      ImGui::SameLine();
      ImGui::RadioButton("Fox  (4)", &brush.kind, BrushSettings::kFox);
      ImGui::SliderFloat("Brush size", &brush.radius, 0.4f, 3.f, "%.1f m");
      ImGui::TextDisabled("Hold the left mouse button to paint; fire and water also\nlight and put out campfires. "
                          "Fox: click to add one in the\nnearest bed that fits");

      if (fps > 0.f)
        ImGui::TextDisabled("%.0f fps (%.1f ms), capped by vsync; --bench measures uncapped", fps, 1000.f / fps);
      else
        ImGui::TextDisabled("measuring fps...");
    }
    ImGui::End();
  }

  // The brush's outline where it lies on the ground, in its colour; the fox
  // tool's in a fox's cream.
  if (brushOutline.size() > 2) {
    static const ImU32 colours[4] = {IM_COL32(255, 150, 70, 220), IM_COL32(120, 190, 255, 220),
                                     IM_COL32(150, 230, 110, 220), IM_COL32(245, 225, 200, 220)};
    std::vector<ImVec2> points;
    for (const glm::vec2& p : brushOutline) points.emplace_back(p.x, p.y);
    ImGui::GetForegroundDrawList()->AddPolyline(points.data(), static_cast<int>(points.size()),
                                                colours[std::clamp(brush.kind, 0, 3)], ImDrawFlags_Closed, 2.f);
  }

  // A word by the cursor, on a dark plate so it reads over any ground.
  if (note) {
    ImDrawList* list = ImGui::GetForegroundDrawList();
    const ImVec2 at{notePoint.x + 18.f, notePoint.y + 10.f};
    const ImVec2 size = ImGui::CalcTextSize(note);
    list->AddRectFilled(ImVec2(at.x - 6.f, at.y - 4.f), ImVec2(at.x + size.x + 6.f, at.y + size.y + 4.f),
                        IM_COL32(20, 18, 24, 210), 4.f);
    list->AddText(at, IM_COL32(255, 235, 220, 255), note);
  }

  // The loading screen, over everything (the menu too): a near-black plate
  // the colour of the night palette's shadows, the title large and warm in
  // the middle, the passage wrapped under it in a muted cream. All of it
  // fades together.
  if (loading && loading->opacity > 0.f) {
    ImDrawList* list = ImGui::GetForegroundDrawList();
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float a = std::clamp(loading->opacity, 0.f, 1.f);
    auto alpha = [&](int full) { return static_cast<int>(static_cast<float>(full) * a + 0.5f); };
    list->AddRectFilled(ImVec2(0.f, 0.f), size, IM_COL32(14, 12, 22, alpha(255)));
    ImFont* font = ImGui::GetFont();
    const float body = ImGui::GetFontSize() * 1.25f;
    const float heading = ImGui::GetFontSize() * 2.6f;
    const float wrap = std::min(size.x * 0.6f, body * 32.f);
    const ImVec2 titleSize = font->CalcTextSizeA(heading, FLT_MAX, 0.f, loading->title);
    const ImVec2 passageSize = font->CalcTextSizeA(body, FLT_MAX, wrap, loading->passage);
    const float gap = body * 1.6f;
    const float top = 0.5f * (size.y - (titleSize.y + gap + passageSize.y));
    list->AddText(font, heading, ImVec2(0.5f * (size.x - titleSize.x), top), IM_COL32(240, 196, 120, alpha(255)),
                  loading->title);
    list->AddText(font, body, ImVec2(0.5f * (size.x - passageSize.x), top + titleSize.y + gap),
                  IM_COL32(214, 204, 186, alpha(255)), loading->passage, nullptr, wrap);
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace game
