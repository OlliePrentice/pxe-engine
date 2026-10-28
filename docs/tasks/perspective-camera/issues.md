## ISSUE-001: Mouse delta depends on display density
Status: resolved
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`Input::mouseDelta()` (`engine/src/input.cpp`, `Input::update`) is the cursor motion multiplied by the framebuffer/window point scale, so it is in framebuffer pixels. The same physical mouse motion reports twice the delta on a 2x high-DPI display, so a game's look speed (a constant radians-per-unit, as in `examples/minimal/main.cpp`'s `kLookPerPixel` and the planned desert game) changes with the display. Turning raw input into device-independent values is the engine's job (`docs/architecture.md`, "Input devices"). Report `mouseDelta` in window points (the cursor's own units, before `pointScale`), document that in `input.hpp` and `docs/engine-guide.md`, and rename the example's constant and comment to match (radians per point). `mousePosition` stays in framebuffer pixels. Re-check byte-identity and that `--first-person --capture` is unchanged (it does not read the mouse).

### Fix notes
Root cause: `Input::update` computed the delta from the cursor position after it was scaled by `pointScale` (framebuffer/window width), so the delta was in framebuffer pixels and doubled on a 2x display. Fix: `Input` now keeps the unscaled cursor position in window points (`mousePoints_`) and takes `mouseDelta_` from that; `mouse_` (what `mousePosition()` returns) is still that position times `pointScale`, so it stays in framebuffer pixels. The first-update and capture-change zeroing is unchanged. `input.hpp`'s comment and `docs/engine-guide.md` (the Input bullet and the Mouse look paragraph) now say window points. The example's constant is renamed `kLookPerPixel` -> `kLookPerPoint` ("radians per window point of mouse motion"), and the guide's frame-loop snippet matches. The value stays 0.0015: on a 2x display the look is half as fast as in cycle 1 and on a 1x display it is the same. Orthographic cases are byte-identical to the base and `--first-person --capture` is byte-identical to cycle 1.
