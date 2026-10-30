# The desert's settings menu
Type: feature
Status: open
Cycle: 1

## Goal
Give `pxe_desert` a Tab settings menu like the ruins game's (`game::SettingsMenu`). The desert gets its own menu on `pxe::Overlay`, with controls only for what the desert actually uses: a running clock, cloud shadows and wind, the player's speeds, field of view and mouse sensitivity, the arms (show or hide, preview the walk and sprint clips, hold a pose), and the rendering switches the perspective frame already has. While the menu is open the cursor is free and the player doesn't look or walk. Every setting defaults to what the desert does today, so play opens unchanged and captures stay byte-identical.

## Decisions
- **Own menu, no shared code.** The menu's contents and `ImGui::` calls are game code (`docs/architecture.md`, "Where things live"). The desert can't include `game/`, so it gets its own `desert::SettingsMenu`, copying what applies from `game/src/settings.cpp`.
- **Running clock.** `desert::Clock` holds the hour, a speed in game hours per real minute (default 6, the ruins' default) and a pause flag. It starts **paused** at `--hour` (default 16.5), so play opens at today's fixed hour until the user unpauses it. The clock is controlled from the menu only: no Space or `[` `]` hotkeys. Hour presets are the ruins': Dawn 5.6, Morning 8.5, Noon 12, Golden 17.6, Dusk 18.6, Night 22.5. Speed presets are the ruins': Real time (1/60), 1 min = 1 h (1), Default (6), Fast (24), Very fast (96).
- **Weather: clouds and wind only.** The menu has cloud cover (0–1, default 0), cloud shadow strength (0–1, default 0.6), wind direction (0–360°, default 20) and wind strength (0–6×, default 1). Cloud shadows show in perspective mode: `bindToon` passes the cloud uniforms to `mesh.frag`, which calls `cloudShade`. Wind only drifts the clouds, because perspective mode draws no grass. **Rain and lightning are left out.** Without falling rain they would only grey the scene.
- **Player tunables.** Walk speed (1–10 m/s, default 3.5), sprint speed (default 6, at least walk + 0.5, at most 15), vertical field of view (40–100°, default 60) and mouse sensitivity (×0.25–×4 on `kLookPerPoint`, default ×1). The gamepad stick turn rates stay fixed. The arms blend walk to sprint by the **live** speeds, not the constants, so the blend still lines up after the speeds change.
- **View model.** A "Show arms" checkbox (default on). A clip preview with three choices, Off (default), Walk or Sprint, plays that clip at the current walk or sprint speed whether or not the player moves, and also while the menu is open. A "Hold pose" checkbox (enabled only while a preview is chosen) with a stride-phase slider (0–1) freezes the previewed clip at that phase at full weight.
- **Rendering: only what the engine already exposes, and no engine changes.** "Edges" (`FrameParams::edges`, default on), "View distance" (`FirstPersonCamera::setFarPlane`, 100–300 m, default 300; the haze starts at 60% of it, so it moves the haze too), and two debug views, "N·L" (`showNdl`) and "Test colours" (`testColours`), both off. The cap is 300 m because `world.hpp` only guarantees the ground's edge is at least 300 m away. There are no new toggles for shadows or haze.
- **Behaviour.**
  - Tab, or the pad's Start button, opens and closes the menu at any time, even while a widget has the keyboard. Opening it frees the cursor. Closing it with Tab, Start or the window's close button captures the cursor again.
  - Escape with the menu open closes it and leaves the cursor free. A second Escape then quits, and a click takes the cursor back, as today. While a menu widget has the keyboard (`wantsKeyboard`), Escape belongs to ImGui and the game doesn't read it.
  - A click in the window while the menu is open never captures the cursor.
  - While the menu is open the player doesn't look or walk, the arms settle as they do with the cursor free (unless a preview is on), and the clock keeps running.
- **`--menu`** starts play with the menu open and the cursor free. It is interactive only: `--menu` together with `--capture` or `--window-png` is a bad argument (exit 1), as `--walk` without a capture is today.
- **Captures are untouched by construction.** The overlay and menu are only created in interactive runs. The headless path keeps today's `pxe::Weather{}`, `opt.hour`, default camera and default `FrameParams`. There is no `--ui-shot` equivalent.
- **Extras.** An fps line at the foot of the menu, as the ruins menu has. Settings don't persist between runs.
- **Standing rule (user decision, 2026-10-30).** Every later desert feature that adds a tunable (sky, haze, clouds, dust, sand, lighting) adds its controls to this menu. It is already recorded in `docs/architecture.md`, "Standing rules per game", by the planning commit.
- **Existing content stays put.** Every default equals today's value. Nothing in the world, the assets or the tuned constants changes.

## Layering
- Engine: none. It uses `pxe::Overlay`, `pxe::Weather`, `pxe::lightingAt`, `pxe::FirstPersonCamera::setFovYRadians` / `setFarPlane`, the `FrameParams` fields `edges`, `showNdl` and `testColours`, and `pxe::Input::gamepadPressed(pxe::GamepadButton::kStart)`, all as they are.
- Game (`desert`):
  - New `desert/src/settings.hpp` and `settings.cpp`: `desert::Clock`, `desert::PlayerSettings`, `desert::ViewSettings` and `desert::SettingsMenu`. `main.cpp` creates and owns one of each, plus a `std::optional<pxe::Overlay>`, in the interactive branch only.
  - `desert::Player` gains live `walkSpeed` and `sprintSpeed` fields. `desert::Arms::walk` takes the speeds, and `Arms` gains `hold` and `release`.
  - `main.cpp` routes Tab, Start and Escape, frees and captures the cursor, and applies the settings to the player, camera, arms and frame each play frame.

## Approach
Work in this order.

### 1. `desert/src/player.hpp` and `player.cpp`
Keep `kWalkSpeed` and `kSprintSpeed` as the defaults. Add to `Player`, after `pitch`:
```cpp
  // Metres a second walking and sprinting: the settings menu's, kWalkSpeed
  // and kSprintSpeed unless it changes them. sprintSpeed > walkSpeed.
  float walkSpeed = kWalkSpeed;
  float sprintSpeed = kSprintSpeed;
```
In `Player::update`, use `(in.sprint ? sprintSpeed : walkSpeed)` in place of the constants. Nothing else changes.

### 2. `desert/src/arms.hpp` and `arms.cpp`
Binding interface changes:
```cpp
  // Advances the walk by the ground distance the eye moved over the last
  // dt seconds (Player::moved, 0 while the player isn't moving), at the
  // player's walk and sprint speeds (Player::walkSpeed, sprintSpeed), which
  // blend the clips by speed. sprintSpeed > walkSpeed.
  void walk(float moved, float dt, float walkSpeed, float sprintSpeed);
  // Holds the arms in one clip at full weight, at stride phase `phase`
  // (0..1): the sprint if `sprint`, else the walk, whatever walk() is told,
  // until release(). walk() keeps easing its weights meanwhile. For
  // inspecting the clips from the settings menu.
  void hold(bool sprint, float phase);
  void release();
```
Add private members `bool held_ = false; bool heldSprint_ = false; float heldPhase_ = 0.f;`.

- `walk()`: replace `kWalkSpeed` with `walkSpeed` and `kSprintSpeed` with `sprintSpeed` in the two `std::clamp` lines. Change nothing else. With the default speeds the arithmetic is identical, so the `--walk` captures stay byte-identical.
- `hold()` sets the three members, with the phase clamped to [0, 1]. `release()` clears `held_`.
- `pose()`: at the top of the skinning choice, if `held_`, sample the held clip (`heldSprint_ ? sprintClip_ : walkClip_`) at `heldPhase_ * model_.clipDuration(clip)` into `pose_` and `model_.skin(pose_, eye, joints_)`. Otherwise keep the existing rest-or-blend code unchanged. The materials and `viewModel` push stay as they are.
- Update the class comment to mention the hold.

### 3. `desert/src/settings.hpp` and `settings.cpp` (new)
Binding declarations (the comments may be reworded; the names, types and defaults may not):
```cpp
#pragma once

#include "player.hpp"

#include "pxe/first_person_camera.hpp"
#include "pxe/lighting.hpp"

#include <glm/vec2.hpp>

namespace pxe {
class Overlay;
}

namespace desert {

// The desert's clock and weather, which the settings menu edits. It starts
// paused, so play opens at the hour --hour gives.
struct Clock {
  float hour = 16.5f;           // 0-24; main sets it from --hour
  float hoursPerMinute = 6.f;   // game hours per real minute: 6 -> a day every 4 minutes
  bool paused = true;
  float cloudCover = 0.f;       // 0 clear, 1 overcast
  float cloudStrength = 0.6f;   // 0 no shadow, 1 as dark as a cast shadow
  float windDegrees = 20.f;     // direction the wind blows toward, compass-style
  float windStrength = 1.f;     // how fast it drifts the clouds
  glm::vec2 cloudOffset{0.f};

  // Advance by `dt` real seconds: the hour and the clouds' drift, both held
  // while paused.
  void advance(float dt);
  // Unit vector, world XZ, the wind blows toward.
  glm::vec2 windDir() const;
  // The weather now, for pxe::lightingAt: clouds and wind, no rain or flash.
  pxe::Weather weather() const;
};

// The player's tunables. Defaults are the game's own.
struct PlayerSettings {
  float walkSpeed = kWalkSpeed;        // metres a second, 1-10
  float sprintSpeed = kSprintSpeed;    // walkSpeed + 0.5 .. 15
  float fovDegrees = 60.f;             // vertical, 40-100
  float lookScale = 1.f;               // times main.cpp's kLookPerPoint, 0.25-4
};

// What the view draws. Defaults are what it draws without the menu.
struct ViewSettings {
  enum ArmsPreview : int { kPreviewOff = 0, kPreviewWalk = 1, kPreviewSprint = 2 };
  bool showArms = true;
  int armsPreview = kPreviewOff;   // play a clip as if walking or sprinting
  bool holdPose = false;           // with a preview: freeze it at holdPhase
  float holdPhase = 0.f;           // 0..1 through the stride
  bool edges = true;               // FrameParams::edges
  bool showNdl = false;            // FrameParams::showNdl
  bool testColours = false;        // FrameParams::testColours
  float viewDistance = pxe::FirstPersonCamera::kDefaultFar;  // metres, 100-300
};

// Dear ImGui panel toggled with Tab (or the pad's Start), built on the
// engine's overlay, drawn at window resolution over the presented view.
class SettingsMenu {
 public:
  // Draw through `overlay`, which must outlive the menu's use; applies the
  // menu's style to it.
  void init(pxe::Overlay& overlay);

  void toggle() { open_ = !open_; }
  void close() { open_ = false; }
  bool open() const { return open_; }
  // True while the menu is open and a widget takes typing.
  bool wantsKeyboard() const;

  // Build and draw the panel (if open). Call every play frame after the
  // scene is presented, before swap.
  void draw(Clock& clock, PlayerSettings& player, ViewSettings& view, float fps);

 private:
  pxe::Overlay* overlay_ = nullptr;
  bool open_ = false;
};

}  // namespace desert
```

`settings.cpp` copies from `game/src/settings.cpp` where it applies:
- `kCloudDrift = 0.05f` and the `clockText` helper, copied, not shared.
- `Clock::advance`: when not paused, `hour = fmod(hour + dt / 60 * hoursPerMinute, 24)` (wrapped below 0 as the ruins do) and `cloudOffset -= windDir() * kCloudDrift * dt * windStrength`.
- `windDir()` is the ruins' formula: `{sin a, -cos a}`.
- `weather()` fills `cloudCover`, `cloudOffset`, `cloudStrength`, `windDir` and `windStrength`, and leaves the rest default.
- `SettingsMenu::init` uses the ruins' style lines exactly (font scale 1.2, `StyleColorsDark`, rounding 6 and 4, window background alpha 0.92).
- `wantsKeyboard` is `overlay_ && open_ && overlay_->wantsKeyboard()`.
- `draw()`: `overlay_->begin()`. If open, draw a window titled `"Settings  (Tab to close)"` with the close button bound to `open_`, `ImGuiWindowFlags_AlwaysAutoResize`, first-use position (16, 16) and width 400. Then `overlay_->end()`. The sections, in this order:
  1. **Time of day**: `Clock  HH:MM` plus `   (paused)` when paused, the "Hour" slider 0–24, and the six hour preset buttons on one row.
  2. **Time speed**: a "Paused" checkbox, a "Game hours / real minute" slider (0.0167–120, logarithmic), the "One day lasts ..." line as the ruins have it, and the five speed preset buttons.
  3. **Clouds and wind**: "Cover", "Shadow strength", "Wind direction" (`%.0f deg`), "Wind strength" (`%.1fx`), then the disabled text "Wind drifts the clouds".
  4. **Player**: "Walk speed" (`%.1f m/s`), then "Sprint speed" (`%.1f m/s`), whose slider minimum is `walkSpeed + 0.5f`. After the walk slider, apply `sprintSpeed = std::max(sprintSpeed, walkSpeed + 0.5f)` every frame. Then "Field of view" (`%.0f deg`) and "Mouse sensitivity" (`%.2fx`, logarithmic).
  5. **Arms**: a "Show arms" checkbox, then radio buttons "Off", "Walk" and "Sprint" on `armsPreview` with the label "Preview" beside them. Then the "Hold pose" checkbox and a "Stride phase" slider (0–1, `%.2f`), both inside `ImGui::BeginDisabled(armsPreview == kPreviewOff)` / `EndDisabled`. Then the disabled text "Preview plays a clip while standing still".
  6. **Rendering**: "Edges", "N·L" and "Test colours" checkboxes on one row, then the "View distance" slider (100–300, `%.0f m`) and the disabled text "Haze starts at 60% of it".
  7. The fps line: `"%.0f fps (%.1f ms)"` when fps > 0, else `"measuring fps..."`. Leave out the ruins' vsync remark.

### 4. `desert/CMakeLists.txt`
Add `src/settings.cpp` to `add_executable`, in alphabetical order. ImGui comes through `pxe_engine`'s public link.

### 5. `desert/src/main.cpp`
- Includes: `"settings.hpp"`, `"pxe/overlay.hpp"`, `<optional>`.
- `Options` gains `bool menu = false;`. `parseArgs` accepts `--menu`. After the existing checks, `opt.menu && headless` prints `pxe_desert: bad argument: --menu is for play, not with --capture or --window-png`, then the usage, and returns 1.
- `printUsage` adds `"  --menu                   start with the settings menu open (Tab)\n"` after `--sprint`. Its Play paragraph and the file's header comment say: Tab (pad: Start) opens and closes the settings menu, which frees the cursor while open, and Escape closes the menu first.
- `readInput` takes a `float lookScale` and multiplies the mouse term only: `input.mouseDelta() * kLookPerPoint * lookScale`.
- `frameParams` takes the weather: `frameParams(const desert::World&, glm::ivec2, float time, float hour, const pxe::Weather& weather)`. The headless loop passes `opt.hour` and `pxe::Weather{}`, so the capture path is otherwise unchanged. The headless walk calls `arms.walk(player.moved, 1.f / 60.f, player.walkSpeed, player.sprintSpeed)`.
- Interactive branch. Sketch: follow its shape and order; the names are binding where they match the declarations above.
```cpp
    pxe::Input input;
    // The settings menu, drawn through the overlay: play only, so captures
    // never touch ImGui. Made after the window, destroyed before it.
    std::optional<pxe::Overlay> overlay;
    overlay.emplace(window);
    desert::SettingsMenu menu;
    menu.init(*overlay);
    desert::Clock clock;
    clock.hour = opt.hour;
    desert::PlayerSettings tuning;
    desert::ViewSettings view;
    if (opt.menu) menu.toggle();
    // Mouse look holds the cursor in the window, unless the menu starts open.
    window.setCursorCaptured(!menu.open());
    // fps: frames counted over each half second.
    ...
    while (!window.shouldClose()) {
      ... dt, pollEvents, input.update as now ...
      const bool wasOpen = menu.open();
      if (input.keyPressed(pxe::Key::kTab) || input.gamepadPressed(pxe::GamepadButton::kStart)) {
        menu.toggle();
      } else if (input.keyPressed(pxe::Key::kEscape) && !menu.wantsKeyboard()) {
        if (menu.open()) {
          menu.close();                         // the cursor stays free
        } else if (window.cursorCaptured()) {
          window.setCursorCaptured(false);
        } else {
          window.requestClose();
        }
      } else if (!menu.open() && !window.cursorCaptured() && input.mousePressed(pxe::MouseButton::kLeft)) {
        window.setCursorCaptured(true);
      }
      // (the menu's close button is handled after draw(), below)
      if (menu.open() != wasOpen) window.setCursorCaptured(!menu.open() && !escapeClosed);
      ...
```
  The rule the code must implement is binding. When the menu goes from closed to open, by any means, release the cursor. When it goes from open to closed by Tab, Start or the window's close button, capture the cursor. When it closes by Escape, leave the cursor free. The window's close button changes `open_` inside `draw()`, so compare `menu.open()` before and after `draw()` and capture the cursor if it went from open to closed there. Implement this cleanly, with a flag for the Escape case or by setting the cursor directly in each branch. The sketch only shows the shape.
- Each frame, after the input handling:
  - `clock.advance(dt)`.
  - Copy `tuning.walkSpeed` and `tuning.sprintSpeed` into `player.walkSpeed` and `player.sprintSpeed`.
  - `const bool playing = window.cursorCaptured() && !menu.open();`. If playing, run `player.update(readInput(input, dt, tuning.lookScale), dt)` and `player.aim(camera)`.
  - The arms' distance: if `view.armsPreview != kPreviewOff`, it is `(armsPreview == kPreviewSprint ? player.sprintSpeed : player.walkSpeed) * dt`. Otherwise it is `player.moved` when playing and 0 when not. Then call `arms.walk(distance, dt, player.walkSpeed, player.sprintSpeed)`.
  - If `view.holdPose && view.armsPreview != kPreviewOff`, call `arms.hold(view.armsPreview == kPreviewSprint, view.holdPhase)`. Otherwise call `arms.release()`.
  - `camera.setFovYRadians(glm::radians(tuning.fovDegrees))` and `camera.setFarPlane(view.viewDistance)`.
  - Build params with `frameParams(world, framebuffer, now, clock.hour, clock.weather())`, then set `params.edges`, `params.showNdl` and `params.testColours` from `view`.
  - Call `arms.pose(view, params)` only if `view.showArms`.
  - `renderer.render(...)`, then `menu.draw(clock, tuning, view, fps)`, then `window.swap()`.
- The minimised-window `continue` stays before any drawing. The overlay's `begin`/`end` must not run on a skipped frame, so `menu.draw` sits after the size check, next to render.
- The local `pxe::PerspectiveFrame` is called `view` in the existing code. Rename it (for example `eyeView`) or name the settings `viewSettings`, so the two don't clash. Your choice; be consistent.

### 6. Comments and docs in the desert
- Update `main.cpp`'s header comment (controls).
- Update `Player`'s and `Arms`' comments where the speeds or hold are described.
- `desert/docs/art-style.md` doesn't change.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `tools/perspective-check.sh <cycle base>`: **3/3 identical** (desert-default, desert-night, example-first-person).
3. Byte-identical walks. Run each on the base build and on the worktree build and `cmp` them; each pair must be identical: `pxe_desert --walk 2 --capture w.png`, `--walk 3 --sprint --capture s.png`, and `--walk 2 --hour 21 --capture n.png`. This proves `walk()` with the live default speeds is unchanged. Use the base build that `perspective-check.sh` leaves in its scratch directory (its output names it), or build the base yourself.
4. `git diff <cycle base> -- engine game examples desert/src/world.hpp desert/src/world.cpp desert/assets desert/tools`: no output.
5. `tools/ortho-check.sh <cycle base>`: 6/6 identical. There are no engine changes, so this is a sanity check.
6. Flags: `build/release/desert/pxe_desert --help` lists `--menu`. `pxe_desert --menu --capture x.png` and `pxe_desert --menu --window-png x.png` each exit 1 with "bad argument".
7. Greps:
   - `git grep -n -e 'ImGui' -e 'imgui' -- desert/src`: matches only in `desert/src/settings.cpp`.
   - `git grep -n -e 'kWalkSpeed' -e 'kSprintSpeed' -- desert/src/arms.cpp`: no output.
   - `git grep -n -e '#include "game' -e '#include "../game' -- desert`: no output.
   - `git grep -n -w Overlay -- desert/src/main.cpp`: only after the `if (headless) { ... return ...; }` block.
8. By reading `main.cpp`:
   - The overlay and menu are constructed after the headless block returns.
   - The headless frames pass `opt.hour` and `pxe::Weather{}`, and touch neither the camera's FOV or far plane nor `params.edges`, `showNdl` or `testColours`.
   - The cursor rule in Decisions ("Behaviour") holds in every branch: Tab, Start, Escape (menu open or closed), a click, and the window's close button.
   - `menu.draw` runs only on frames that render.
9. Put the playtest checklist below in the handoff, for the user. Nothing automated drives the keyboard, mouse or pad.

### Playtest checklist (for the user)
- Play starts exactly as before: cursor held, 16:30, no clouds, arms at rest. `--menu` starts with the menu open and the cursor free.
- Tab opens the menu and frees the cursor, and mouse look and WASD stop. Tab closes it and the cursor is held again. The same with the pad's Start button. The window's ✕ closes it and recaptures the cursor.
- Escape with the menu open closes it and leaves the cursor free. Escape again quits. A click instead recaptures the cursor. Clicking the menu's widgets never recaptures it.
- Unpause the clock: the light moves through the day, and the presets and speed buttons work.
- Raise cloud cover: cloud shadows cross the sand and the palm, drifting with the wind direction and strength.
- Change walk and sprint speed and walk: the speed changes and the arms still swing in step. The sprint slider can't go below walk + 0.5.
- Field of view, mouse sensitivity and view distance (the haze moves in) each take effect live.
- Arms: hide and show. Preview Walk and Sprint while standing still. Hold pose and scrub the phase. Preview Off returns the arms to rest.
- Edges, N·L and Test colours toggle as expected. The fps line updates.

## Acceptance criteria
- [ ] `desert/src/settings.hpp` declares `Clock`, `PlayerSettings`, `ViewSettings` and `SettingsMenu` with the binding names, types and defaults. `settings.cpp` implements them as the Approach describes, with the sections in the given order and no rain, lightning, sound, brush or fire controls.
- [ ] `Player` has `walkSpeed` and `sprintSpeed` defaulting to the constants, and `update` uses them.
- [ ] `Arms::walk` takes the speeds and is otherwise unchanged. `hold` and `release` work as specified, and `pose()` draws the held clip at full weight while held.
- [ ] `main.cpp` implements Tab, Start, Escape, click and close-button behaviour exactly as Decisions ("Behaviour") says. The player doesn't look or walk while the menu is open, and the clock advances every play frame.
- [ ] Every setting is applied in interactive frames as the Approach lists. The headless path uses `opt.hour`, `pxe::Weather{}` and the default camera and `FrameParams`.
- [ ] `--menu` parses, appears in `--help`, starts with the menu open and the cursor free, and is rejected with `--capture` or `--window-png`.
- [ ] Verification 2–7 pass: perspective 3/3, the three walk captures identical to base, the diff empty, ortho 6/6, the flags and greps as stated.
- [ ] No file under `engine/`, `game/` or `examples/` changed.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Rain, lightning and sound controls. Anything for grass, fires, brushes or foxes.
- New engine toggles (shadows on or off, a haze switch, haze colour or span apart from the far plane).
- Clock hotkeys (Space, `[`, `]`). A `--ui-shot`-style screenshot of the menu.
- Saving settings between runs.
- Gamepad navigation inside the menu (ImGui nav). The pad only opens and closes it.
- Sharing code with `game::SettingsMenu`.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base 1722db0
