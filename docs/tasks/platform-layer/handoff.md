## Cycle 1
Build: pass

### Changes
- engine/include/pxe/window.hpp, engine/src/window.cpp: new `pxe::Window` (+ `WindowOptions`: width, height, title, visible, vsync). Starts GLFW, sets today's hints (GL 4.1 core, forward compat, no MSAA, no sRGB, retina framebuffer, hidden when `visible` is false), makes the context current, sets the swap interval, calls `pxe::loadGl`. shouldClose, requestClose, pollEvents, swap, framebufferSize, windowSize, time, setTitle. RAII destroy + terminate; non-copyable; throws `std::runtime_error` with today's two messages ("glfwInit failed", "failed to create an OpenGL 4.1 window").
- engine/src/window_impl.hpp: private `Window::Impl` holding the `GLFWwindow*`; `Input` and `Overlay` are friends of `Window` and read it through here. No public header names a GLFW type.
- engine/include/pxe/input.hpp, engine/src/input.cpp: new `pxe::Input` with enums `Key` (A-Z, 0-9, F1-F12, arrows, space, tab, escape, enter, backspace, insert/delete/home/end/page up/down, both shifts/controls/alts/supers, brackets and the other punctuation keys), `MouseButton`, `GamepadButton` (all 15 GLFW gamepad buttons), `GamepadStick`, `GamepadAxis`. `update(const Window&)` reads everything once a frame and keeps the previous frame for edges. Mouse position in framebuffer pixels (cursor * fbW/winW, the same arithmetic main.cpp used). Gamepad device code moved here unchanged: the extra mappings string and its one-time `glfwUpdateGamepadMappings`, first connected gamepad, the radial dead zone (0.18, same formula, now `Input::kStickDeadZone` / `gamepadStick`), the connect/disconnect log line.
- engine/include/pxe/overlay.hpp, engine/src/overlay.cpp: new `pxe::Overlay`: creates the ImGui context (IniFilename = nullptr as an engine default), inits the GLFW (install_callbacks = true) and OpenGL3 ("#version 410") backends; begin() = OpenGL3/Glfw NewFrame + ImGui::NewFrame; end() = Render + RenderDrawData; wantsKeyboard() = WantTextInput, wantsMouse() = WantCaptureMouse. Destructor shuts down OpenGL3, Glfw, then destroys the context.
- engine/include/pxe/gl_loader.hpp: comment no longer names glfwGetProcAddress.
- CMakeLists.txt: `imgui` target is core only (no backends, no glfw link, no backends include dir).
- engine/CMakeLists.txt: new sources; compiles imgui_impl_glfw.cpp and imgui_impl_opengl3.cpp (with `-w`, as miniaudio is); backends include dir PRIVATE; links `imgui` PUBLIC and `glfw` PRIVATE.
- game/CMakeLists.txt: links only `pxe_engine`.
- game/src/main.cpp: one `pxe::Window` for every mode (visible = !headless && no --ui-shot; vsync = opt.vsync && !headless, as before); --audio-wav still returns before it. Bench timing, play-loop timing/fps/dt clamp use `window->time()`; sizes, title, swap, close go through the window. `pollInput(const pxe::Input&, bool keyboardTaken, game::Gamepad&)` has no statics; same key -> action mapping. The brush/fox mouse code reads `mousePosition()`, `mouseDown(kLeft)` and `mousePressed(kLeft)` (replacing the `leftHeld` latch, same semantics). Scene-build failure now relies on RAII (renderer is now destroyed before the window, where before it outlived glfwTerminate).
- game/src/gamepad.hpp/.cpp: `game::Gamepad` is now only the action mapping over `pxe::Input` (move, LB/RB, flick, X, Start, Back); flick thresholds 0.65/0.3 unchanged and still game constants.
- game/src/settings.hpp/.cpp: `SettingsMenu::init(pxe::Overlay&)` applies the style (font scale 1.2, StyleColorsDark, rounding 6/4, window bg alpha 0.92); `shutdown()` removed (Overlay is RAII); `draw` wraps its widgets in `overlay.begin()`/`end()`; wantsKeyboard/wantsMouse = open && overlay wants it.
- README.md: framebuffer-size note names `pxe::Window::framebufferSize` instead of glfwGetFramebufferSize.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: `pxe::Window`, `pxe::WindowOptions`, `pxe::Input` (+ `Key`, `MouseButton`, `GamepadButton`, `GamepadStick`, `GamepadAxis`), `pxe::Overlay`; private `Window::Impl`.
- Game: `main()` creates and owns the `Window` (std::optional, for the error path), the play loop owns the `Overlay` and the `pxe::Input`; `game::Gamepad` and `pollInput` map raw input to actions; `SettingsMenu` keeps its style and widgets.
- Misplaced code touched: this task is the refactor for the window/input/overlay rows of docs/architecture.md; those rows can now be marked in place (left for the orchestrator). Also touched, unchanged in placement: `writePng` and `lightingFor` call sites in game/src/main.cpp (only the surrounding window calls changed).

### Verification
Base: `git archive 75f49c9` extracted to /private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/d14f2cb1-88f9-453a-96ce-d0bdb809fbd5/scratchpad/base, Debug build. New: worktree Debug build. Both run from /private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/d14f2cb1-88f9-453a-96ce-d0bdb809fbd5/scratchpad/out.
- `pxe --capture <v>-h15.png --hour 15`: cmp identical
- `pxe --capture <v>-h6r.png --hour 6 --rain 0.3`: cmp identical
- `pxe --capture <v>-h21.png --hour 21`: cmp identical
- `pxe --window-png <v>-win.png`: cmp identical
- `pxe --self-test` (new): PASS, exit 0
- `pxe --bench 60` (new): completed, "bench: wall 2.77 ms/frame (361 fps uncapped), 1920x1080"
- `pxe --ui-shot new-ui.png --menu` (new): wrote 1920x1080 PNG showing the settings menu, same look as the base's. Not byte-comparable: the play loop runs on wall-clock time, and two base runs differ from each other too (only the flame differs visually).
- Fresh configure + build into an empty directory: pass; the only warning is the pre-existing stb_image_write sprintf deprecation (the base's "ignoring duplicate libraries: libglfw3.a" linker warning is gone).
- `grep -rn "GLFW\|glfw" game/` and `grep -rn "ImGui_Impl\|imgui_impl" game/`: nothing. No engine/include header mentions GLFW.

### How to playtest
- Play: WASD/arrows move, Shift walks, X attacks (hold to charge), Q/E rotate, Space pauses, [ ] speed, R rain, L strike, M sound, G grass fire, F fireball + IJKL, C follow, 1-4 brushes, F1-F4, Esc quits. All should act as before.
- Tab and the pad's Start toggle the menu; with a slider/text field being typed into (Ctrl+click a slider), only Tab/Start work.
- Brushes: hold left mouse to paint; fox tool (4) adds a fox per click, not per held frame; clicks over the menu don't paint.
- Gamepad: connect/disconnect logs as before; left stick moves with the same dead zone; right-stick flick and LB/RB rotate; X attacks; Back pauses.
- Window title updates with the clock; resizing still keeps the image integer-scaled.

### Known limitations
- Accepted behaviour change (per spec): key edges are tracked every frame, including while the menu has the keyboard, so a key held when focus returns no longer fires then.
- Spec deviation to flag: `pxe::Input` exposes raw axes (`gamepadAxis`) as well as dead-zoned sticks (`gamepadStick`). Today's right-stick flick reads the raw axis without the dead zone; feeding it the dead-zoned stick would shift the effective flick thresholds (0.65 -> about 0.71 raw), so the game reads the raw axis to keep behaviour identical.
- `gamepadAxis` reports triggers as GLFW does (-1 released .. 1 pulled); unused by the game.
- Not playtested interactively (no human input available here); keyboard/mouse/gamepad mapping checked by reading against the base code.

## Cycle 2
Build: pass

### Changes
- Merge commit a6af8d9: `git merge engine-game-layering` into platform-layer (no rebase), bringing in weather-lighting and firegrid-scene-pointer.
- game/src/settings.cpp: conflict resolved keeping both sides: `WorldClock::weather()` from weather-lighting, followed by this task's `SettingsMenu::init(pxe::Overlay&)` (no ImGui context creation or backend calls in the game).
- game/src/main.cpp, game/src/settings.hpp: auto-merged. main.cpp keeps the `pxe::Window` / `pxe::Input` / `pxe::Overlay` changes and all four `pxe::lightingAt(scene, clock.hour, clock.weather())` call sites, plus `pxe::lightingAt(scene, kCaptureHour)` on the capture path (as on engine-game-layering). `lightingFor` is gone.

### Issues addressed
- None (merge-back cycle; no issues in issues.md).

### Layering
- Engine: none new this cycle (merged-in `pxe::Weather` / `pxe::lightingAt` come from weather-lighting).
- Game: unchanged from cycle 1.
- Misplaced code touched: none.

### Verification
- Fresh configure + Debug build into an empty build directory: pass; only warning is the pre-existing stb_image_write sprintf deprecation.
- `grep -rn "GLFW\|glfw\|ImGui_Impl\|imgui_impl" game/`: empty.
- `pxe --self-test`: PASS.
- `pxe --capture merge-h15.png --hour 15`: wrote the PNG; visually checked, renders the ruins scene normally.

### How to playtest
- As cycle 1. Additionally check that rain (R) and lightning (L) still darken and flash the scene, since lighting now takes the weather from `WorldClock::weather()`.

### Known limitations
- Same as cycle 1. Not playtested interactively.
