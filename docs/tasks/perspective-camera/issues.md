## ISSUE-001: Mouse delta depends on display density
Status: open
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`Input::mouseDelta()` (`engine/src/input.cpp`, `Input::update`) is the cursor motion multiplied by the framebuffer/window point scale, so it is in framebuffer pixels. The same physical mouse motion reports twice the delta on a 2x high-DPI display, so a game's look speed (a constant radians-per-unit, as in `examples/minimal/main.cpp`'s `kLookPerPixel` and the planned desert game) changes with the display. Turning raw input into device-independent values is the engine's job (`docs/architecture.md`, "Input devices"). Report `mouseDelta` in window points (the cursor's own units, before `pointScale`), document that in `input.hpp` and `docs/engine-guide.md`, and rename the example's constant and comment to match (radians per point). `mousePosition` stays in framebuffer pixels. Re-check byte-identity and that `--first-person --capture` is unchanged (it does not read the mouse).

### Fix notes
