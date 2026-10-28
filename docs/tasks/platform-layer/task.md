# Platform layer: window, input and UI overlay in the engine
Type: feature
Status: complete
Cycle: 2

## Goal
Move everything that talks to GLFW, and ImGui's GLFW and OpenGL backends, from the game into the engine, as three engine capabilities: a window with its timing, raw input from keyboard, mouse and gamepad, and a UI overlay that hosts Dear ImGui. docs/architecture.md marks all three engine and misplaced. Afterwards the game never includes GLFW or calls an ImGui backend: it asks the engine for a window, reads raw input from it and maps it to actions, and builds its settings menu with `ImGui::` widget calls between the overlay's begin and end. Every run mode behaves as it does today.

## Decisions
- Window, input and overlay are one task: once the window hides GLFW, the game can't read keys or set up ImGui without the other two, so splitting them leaves a half-moved state.
- No raw GLFW handle in the engine's public API. No engine public header includes GLFW or names a GLFW type; keep GLFW behind a private implementation (pimpl or an opaque pointer defined in engine/src).
- The engine owns ImGui's context and backends (best practice: the engine owns the platform and graphics API; the game only uses the ImGui core API). ImGui core is a general UI library both layers may use, so `pxe_engine` links it PUBLIC.
- The game keeps its menu's look: font scale, rounding and colours stay in game code, applied through the ImGui core API after the overlay is set up. `io.IniFilename = nullptr` becomes an engine default.
- Input is polled state, not callbacks: the engine reads key, mouse and gamepad state once per frame and tracks edges against the previous frame inside `pxe::Input`. ImGui's backend keeps installing its own GLFW callbacks (`install_callbacks = true`), set up after the window, as today.
- Keys are an engine enum (`pxe::Key`) covering the keyboard generally (letters, digits, F1-F12, arrows, space, tab, escape, enter, backspace, both shifts, controls and alts, brackets), not just the keys this game uses. Gamepad buttons and axes are engine enums too.
- Device-level gamepad code moves to the engine: the extra controller mappings string and its one-time `glfwUpdateGamepadMappings`, picking the first connected gamepad, the radial dead zone (values unchanged), and the connect/disconnect log line. Action mapping stays in the game: X attacks, Start toggles the menu, Back pauses, bumpers and the right-stick flick rotate (flick thresholds stay game constants, values unchanged).
- Accepted behaviour change: edges are tracked every frame, including while the menu has the keyboard. Today a key's latch freezes while the keyboard is taken, so a key held when focus returns fires then; afterwards it won't. Everything else about input behaves as today: the same keys and buttons do the same things, Tab and Start toggle the menu even while the keyboard is taken, and nothing else is read while it is.
- Engine errors follow `Renderer::init`: throw `std::runtime_error`, and the game prints and exits 1 as it does now.

## Layering
- Engine:
  - `pxe::Window` (engine/include/pxe/window.hpp): creates GLFW and an OpenGL 4.1 core window with today's hints (forward compat, no MSAA, no sRGB, retina framebuffer), options for width, height, title, visible and vsync; makes the context current and loads GL (`pxe::loadGl`). Offers should-close and request-close, poll events, swap, framebuffer size, window size, time in seconds, and set title. RAII: destroys the window and terminates GLFW. Non-copyable.
  - `pxe::Input` (engine/include/pxe/input.hpp): updated once per frame from the window. Key down and pressed (edge); mouse position in framebuffer pixels, left/right/middle button down and pressed; the first connected gamepad: connected, name, button down and pressed, stick axes after the radial dead zone, trigger axes.
  - `pxe::Overlay` (engine/include/pxe/overlay.hpp): created from a `Window`; owns the ImGui context and the GLFW and OpenGL3 backends; `begin()` starts a UI frame, `end()` renders it over the presented frame; `wantsKeyboard()` (ImGui's WantTextInput) and `wantsMouse()` (WantCaptureMouse). RAII shutdown in the right order.
  - CMake: `pxe_engine` gains the new sources, links glfw PRIVATE, and compiles ImGui's `imgui_impl_glfw.cpp` and `imgui_impl_opengl3.cpp` itself. The root `imgui` target becomes core only (no backends, no glfw link). `pxe_engine` links `imgui` PUBLIC.
- Game:
  - `main.cpp` creates one `pxe::Window` (hidden for the headless modes and `--ui-shot`, vsync as today) and uses it for the loop, timing, sizes, title and swap, in every mode: play, `--bench`, `--self-test`, `--capture`, `--window-png`, `--ui-shot`. `--audio-wav` still runs before any window is made.
  - `pollInput(const pxe::Input&, bool keyboardTaken)` maps raw input to the game's `Input` actions, with no `static` latches. The mouse code in the play loop reads the cursor and left button from `pxe::Input`.
  - `game::Gamepad` (gamepad.hpp/.cpp) shrinks to the action mapping over `pxe::Input`'s gamepad state, or folds into `pollInput`; its device code is gone.
  - `SettingsMenu` takes the `pxe::Overlay` (or the `Window` to make one), applies its style, and in `draw` builds its widgets between `overlay.begin()` and `overlay.end()`. `wantsKeyboard`/`wantsMouse` keep today's meaning: open and the overlay wants it.
  - game/CMakeLists.txt no longer links glfw.

## Acceptance criteria
- [x] `grep -rn "GLFW\|glfw" game/` finds nothing (GLFW mentioned in a comment is fine if it explains history, but there should be no includes, types or calls). (grep finds nothing)
- [x] `grep -rn "ImGui_Impl\|imgui_impl" game/` finds nothing. (grep finds nothing)
- [x] No header under engine/include/ includes `<GLFW/glfw3.h>` or names a GLFW type. (GLFW only in engine/src/window_impl.hpp, input.cpp, window.cpp, overlay.cpp)
- [x] `pxe::Window`, `pxe::Input` and `pxe::Overlay` exist as described under Layering, with no game content in their names, APIs or comments. (engine/include/pxe/window.hpp, input.hpp, overlay.hpp)
- [x] Window hints, vsync rule (on only when requested and not headless) and the hidden window for headless modes and `--ui-shot` match today's main.cpp:877-891. (Window::Window hints; main.cpp visible = !headless && uiShot empty, vsync = opt.vsync && !headless)
- [x] `pollInput` has no `static` variables, and every key and button maps to the same action as today's main.cpp:427-497 and gamepad.cpp:36-78. Tab and Start toggle the menu even when the keyboard is taken; nothing else is read then. (main.cpp pollInput over pxe::Input; Tab/Start before the keyboardTaken return)
- [x] Dead zone, extra controller mapping string and flick-to-rotate thresholds have the same values as today. (Input::kStickDeadZone 0.18, kExtraMappings in input.cpp, kFlickOut/kFlickBack in gamepad.cpp; flick still reads the raw axis, as before)
- [x] The play loop's frame timing, fps average and dt clamp (main.cpp:1303-1315) behave as today, using `Window` time. (main.cpp loop uses Window::time with the same fps average and clamp)
- [x] The menu's ImGui style is unchanged (font scale 1.2, rounding 6 and 4, window background alpha 0.92, dark colours). (SettingsMenu::init; orchestrator viewed --ui-shot --menu: menu styled as before)
- [x] Root CMakeLists `imgui` target is core only; `pxe_engine` compiles the two backends and links glfw PRIVATE and imgui PUBLIC; the game target links neither glfw nor the backends. (CMakeLists.txt imgui core only; engine/CMakeLists.txt backends, glfw PRIVATE, imgui PUBLIC; game links pxe_engine only)
- [x] Captures are byte-identical before and after: build the cycle base, run `--capture` at `--hour 15`, `--hour 6 --rain 0.3` and `--hour 21`, and `--window-png` once, then the same on the new build, and `cmp` each pair. Record the commands and results in the handoff. (developer: four cmp equal; orchestrator re-ran --hour 6 --rain 0.3 capture and --window-png against 75f49c9: cmp equal)
- [x] `--self-test` passes and `--bench 60` runs to completion on the new build; `--ui-shot` with the menu open (`--menu`) writes a PNG showing the menu. Record in the handoff. (developer and orchestrator: self-test PASS; developer: bench 60 ran; ui-shot written)
- [x] Project builds cleanly from a fresh build directory with CMake. (fresh build-check in worktree, exit 0; glfw duplicate-library link warning gone)

## Assets
None

## Out of scope
- Moving PNG writing, audio, lighting or scene building (separate tasks).
- New input features (rebinding, more than one gamepad, text input).
- Any change to what the menu shows or how it looks.

## Cycle log
- Cycle 1: 2026-10-26 started, base 75f49c9 (worktree /Users/sampr/Projects/Games/pxe/pxe-engine-platform-layer, branch platform-layer)
- Cycle 1: review passed, no issues; complete. Accepted deviation: the right-stick flick reads the raw axis (pxe::Input::gamepadAxis), keeping today's threshold
- Cycle 2: 2026-10-26 started, base f4f95e7: merge engine-game-layering into platform-layer; conflict in game/src/settings.cpp (WorldClock::weather() from weather-lighting next to SettingsMenu::init)
- Cycle 2: review passed: settings.cpp keeps WorldClock::weather() and SettingsMenu::init(pxe::Overlay&); fresh build exit 0; --hour 19 --rain 0.6 capture byte-identical to engine-game-layering 0e6e47b; complete
