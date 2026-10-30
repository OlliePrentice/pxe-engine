# The desert's light and its menu controls
Type: feature
Status: complete
Cycle: 1

## Goal
Give the desert its own warm ground bounce under the perspective mode's new smooth lighting (`perspective-smooth-lighting`), and add a "Lighting" section to the desert's settings menu (`desert-settings-menu`). The section lets the user tune the light, sky and haze live and print the result as code to paste back into `world.cpp`. Then bring `desert/docs/art-style.md` up to date for the smooth look.

## Decisions
- **The desert's starting values** are the engine defaults (`perspective-smooth-lighting`, Decisions table), except the ground bounce, which takes the sand's warmth. Bounce is sRGB colour × linear strength:

  | day | golden | twilight | night |
  |---|---|---|---|
  | (214,176,118) × 0.12 | (220,160,110) × 0.10 | (150,120,130) × 0.05 | (60,65,85) × 0.03 |

  `desert::buildWorld` sets only these. Everything else stays at the engine defaults. The user tunes it afterwards in the menu.
- **Lighting section of the menu.** Under the dominant phase now (the largest `Lighting::phaseWeight`, shown by name: Day, Golden, Twilight or Night) the section has:
  - colour pickers and strength sliders for the sun (moon at night), the sky fill and the ground bounce, and colour pickers for the sky's horizon and zenith;
  - below those, haze density, sun glow, shadow penumbra and exposure;
  - a "Print values" button that writes all four phases' light and sky colours and strengths, plus the four single values, to stdout as C++ statements ready to paste into `desert::buildWorld`.

  The menu edits the desert's own `Scene` directly. `pxe::lightingAt` reads it fresh every frame, so the edits take effect at once.
- **Edits don't persist.** Print and paste is the way to keep them.
- **Captures:** `buildWorld` sets the bounce, so desert captures change from the `perspective-smooth-lighting` result, in the bounce only. The menu is play-only, so captures don't depend on it.
- **Standing rule** (`docs/architecture.md`): the desert's lighting tunables live in this menu.

## Layering
- Engine: none. It uses the `Scene` fields and `Lighting::phaseWeight` that `perspective-smooth-lighting` adds.
- Game (`desert`):
  - `world.cpp` sets the ground bounce.
  - `settings.hpp`/`settings.cpp` (from `desert-settings-menu`) gain the Lighting section and the print.
  - `main.cpp` passes the scene and this frame's `Lighting` to `SettingsMenu::draw`.

## Approach
1. **`desert/src/world.cpp`, `buildWorld`.** After the scene's existing setup, where the comment says it keeps the engine's default palettes, set `scene.groundBounce[ph]` and `scene.groundBounceStrength[ph]` for the four phases from the table. Update that comment: the desert keeps the engine's palettes and perspective light, apart from a warm sand bounce.
2. **`desert/src/settings.hpp`.** Change `SettingsMenu::draw`'s signature (binding):
   ```cpp
   void draw(Clock& clock, PlayerSettings& player, ViewSettings& view, pxe::Scene& scene,
             const pxe::Lighting& lighting, float fps);
   ```
   Add `#include "pxe/scene.hpp"`. `pxe/lighting.hpp` is already included.
3. **`desert/src/settings.cpp`.**
   - Add the section **"Lighting"** after "Rendering" and before the fps line.
   - The phase `ph` is the index of the largest `lighting.phaseWeight[i]`; the first index wins a tie. Show the line `Editing: <Day|Golden|Twilight|Night>` with the disabled text "the phase in effect now; change the hour for the others".
   - For phase `ph`:
     - `ImGui::ColorEdit3("Sun##c", &scene.sunColour[ph].x)` and `SliderFloat("Sun strength", &scene.sunStrength[ph], 0, 3, "%.2f")`. At night label them "Moon" and "Moon strength"; the IDs stay the same.
     - `ColorEdit3("Sky fill", ...)` and `SliderFloat("Sky fill strength", ..., 0, 1.5, "%.2f")`.
     - `ColorEdit3("Ground bounce", ...)` and `SliderFloat("Bounce strength", ..., 0, 1, "%.2f")`.
     - `ColorEdit3("Sky horizon", &scene.skyHorizon[ph].x)` and `ColorEdit3("Sky zenith", &scene.skyZenith[ph].x)`.
   - Then the single values:
     - "Haze density": `SliderFloat`, 0–0.03, `%.4f /m`;
     - "Sun glow": 0–1;
     - "Shadow penumbra": 0–1, `%.2f m`;
     - "Exposure": 0.25–4, logarithmic, `%.2fx`.
   - **"Print values"** button: writes to `stdout` exactly this shape (binding format; `%.3f` for colour channels, `%.2f` for strengths, `%.4f` for haze density, `%.2f` for the others), all four phases in order Day, Golden, Twilight, Night, and flushes:
     ```
     // pxe_desert lighting: paste into desert::buildWorld
     scene.sunColour[pxe::kPhaseDay] = {1.000f, 0.957f, 0.878f};
     scene.sunStrength[pxe::kPhaseDay] = 1.00f;
     scene.skyFill[pxe::kPhaseDay] = {...};
     scene.skyFillStrength[pxe::kPhaseDay] = 0.30f;
     scene.groundBounce[pxe::kPhaseDay] = {...};
     scene.groundBounceStrength[pxe::kPhaseDay] = 0.12f;
     scene.skyHorizon[pxe::kPhaseDay] = {...};
     scene.skyZenith[pxe::kPhaseDay] = {...};
     ... the same eight lines for kPhaseGolden, kPhaseTwilight, kPhaseNight ...
     scene.hazeDensity = 0.0060f;
     scene.sunGlow = 0.60f;
     scene.shadowPenumbra = 0.30f;
     scene.exposure = 1.00f;
     ```
     Show the disabled text "Prints all four phases to the terminal as code for world.cpp".
   - In the "Rendering" section, the disabled text under "View distance" changes from "Haze starts at 60% of it" to "The haze is full at it".
4. **`desert/src/main.cpp`.** Keep this frame's `pxe::FrameParams params` in scope until the menu draws, and call `menu.draw(clock, tuning, view, world.scene, params.lighting, fps)`. Use whatever names `desert-settings-menu` settled on. `world` is already non-const in `main`.
5. **`desert/docs/art-style.md`.** Make exactly these replacements and nothing else:
   - In "Look", replace the whole bullet
     > - The engine lights everything with a hard three-tone toon ramp. Choose base colours as the mid tone; the engine makes the light and shadow tones.

     with
     > - The engine lights everything smoothly (the perspective mode's lit model): a warm direct sun or moon with soft-edged shadows, a cooler sky fill from above, a bounce from the ground below, and continuous distance haze that glows toward the sun. The mood reference is `desert/docs/concept/wanderers.png`: a low warm sun through dusty haze. Choose each base colour as the material's albedo, how it looks facing the noon sun. The time of day comes from the scene's light colours, not from the material.
   - In "Terrain", replace
     > built on a 1 m grid with smooth normals, so the toon ramp cuts each slope into a few clean bands that follow its curve.

     with
     > built on a 1 m grid with smooth normals, so the light falls off continuously across each slope's curve.
   - In "Palette", replace the whole Sand bullet
     > - **Sand:** a warm ochre mid tone, about sRGB (214, 176, 118), by day. Golden hour pushes it orange, twilight pink-mauve, and night cool blue-grey. The engine holds these as `Sand`'s default palettes.

     with
     > - **Sand:** a warm ochre albedo, about sRGB (214, 176, 118) (`Scene::albedo[kSurfSand]`, the engine's default). The time of day colours it through the light: an orange sun at golden hour, pink-mauve at twilight, a cool blue-grey moon at night. The ground bounces the sand's own warmth back up. The desert's light, sky and haze values are set in `desert::buildWorld` (`desert/src/world.cpp`), and tuned live in the settings menu's Lighting section, whose "Print values" writes them out as code.

   Everything else in the file (view models, characters, the framing rules, budgets) stays word for word.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `git diff <cycle base> -- engine game examples`: no output. `tools/ortho-check.sh <cycle base>`: 6/6 identical.
3. `tools/perspective-check.sh <cycle base>`:
   - `example-first-person` is **identical**;
   - `desert-default` and `desert-night` **differ**, because the bounce changed.
4. **Bounce-only proof.** Temporarily replace the whole block of `groundBounce`/`groundBounceStrength` assignments in `desert/src/world.cpp` with nothing (delete them). Rebuild and capture `pxe_desert --capture nobounce.png`, then restore the file and `touch` it. That capture must be `cmp`-identical to the cycle base's `desert-default` capture (from the `perspective-check.sh` scratch output). A rebuilt capture with the bounce restored must differ from `nobounce.png`.
5. Captures into `docs/tasks/desert-lighting/`, each run twice and `cmp`-identical: `pxe_desert --capture d1630.png`, `--hour 18.3 --capture d1818.png` and `--hour 21 --capture d21.png`. Faces turned down and slopes away from the sun are a touch warmer than in `docs/tasks/perspective-smooth-lighting/` captures of the same hour. Describe each.
6. `git grep -n "Haze starts at 60%" -- desert`: no output. `git diff <cycle base> -- desert/docs/art-style.md` shows exactly the three replacements.
7. By reading `settings.cpp`:
   - The Lighting section edits `scene` fields for the dominant phase only.
   - The labels switch to Moon at night.
   - "Print values" writes the binding format for all four phases.
8. Add to the handoff, for the user's playtest:
   - Open the menu at a few hours and see the phase name follow the clock.
   - Drag each control and see it take effect at once: sun, fill, bounce, horizon, zenith, haze, glow, penumbra, exposure.
   - Press "Print values", paste the output into `buildWorld` in place of the bounce block, and confirm it builds and looks the same.

## Acceptance criteria
- [x] `buildWorld` sets the four phases' ground bounce from the Decisions table, and nothing else of the lighting. Evidence: `desert::buildWorld` in `desert/src/world.cpp`; Verification 4 (no-bounce capture identical to base).
- [x] `SettingsMenu::draw` has the binding signature. The Lighting section has the controls, ranges, phase choice and Moon labels specified, and edits the scene live. Evidence: `desert/src/settings.hpp`, Lighting section of `SettingsMenu::draw` in `settings.cpp` (`###sunColour`/`###sunStrength` IDs keep the ID fixed across the Sun/Moon label, as the spec requires; `##c` would not).
- [x] "Print values" writes the binding format for all four phases and the four single values. Evidence: `printLighting` in `desert/src/settings.cpp`.
- [x] The View distance note reads "The haze is full at it". Evidence: `settings.cpp`; Verification 6 grep empty.
- [x] `desert/docs/art-style.md` has exactly the three replacements. Evidence: `git diff 8f2a3a9 -- desert/docs/art-style.md`, 3 lines changed.
- [x] Verification 2–6 pass. Evidence: orchestrator's ortho-check 6/6; perspective-check example identical, desert-default/night differ; developer's bounce-only proof and repeat captures in handoff.md.
- [x] Project builds cleanly from a fresh build directory with CMake Evidence: orchestrator's `check` preset build, only stb `sprintf` warning.

## Assets
None.

## Out of scope
- Engine changes of any kind.
- Saving edits to a file or reading them back; editing palettes, rim or ink colours; per-surface albedo controls.
- Changing the sand, bark or leaf albedo.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base 8f2a3a9
- Cycle 1 review: build clean, checks as specified, diff matches the spec. Sun/Moon widgets use `###` IDs so the ID stays the same, as Approach 3 requires. No issues. Complete.

## Playtest checklist
- `tools/build.sh release desert`, press Tab: the Rendering section's note reads "The haze is full at it", and a Lighting section follows it.
- Set the hour to about 12, 18.3, 19.5 and 23: "Editing:" follows the clock (Day, Golden, Twilight, Night), and at night the first two controls read "Moon" and "Moon strength".
- Drag each control and see it take effect at once: sun, sun strength, sky fill and its strength, ground bounce and its strength, sky horizon, sky zenith, haze density, sun glow, shadow penumbra, exposure (logarithmic).
- Look down at the sand on a slope facing away from the sun: a slight warm lift from the bounce compared with before.
- Press "Print values": the terminal shows 32 phase lines (Day, Golden, Twilight, Night) and the four single values. Paste them into `buildWorld` in place of the bounce block, rebuild, and confirm it builds and looks the same.
