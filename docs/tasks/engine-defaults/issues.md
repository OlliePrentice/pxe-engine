## ISSUE-001: Minimising the window on Windows makes render throw
Status: fixed
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
Raised by the developer in handoff.md, cycle 1. Before this task, `Renderer::drawPresent` clamped a zero framebuffer size to 1. Now `Renderer::render` throws `std::invalid_argument` when `present` is set and a size is not positive (engine/src/renderer.cpp, start of `Renderer::render`). GLFW reports a 0x0 framebuffer for a minimised (iconified) window on Windows, so minimising would make the game's interactive loop (game/src/main.cpp) and the example's window loop (examples/minimal/main.cpp) throw and exit.

Required fix (decided in review, recorded in task.md `## Decisions`): the engine keeps the throw. The game's interactive loop and the example's window loop skip drawing while `Window::framebufferSize()` has a zero dimension: no `render`, no overlay frame, no swap, but events are still polled and the game keeps updating as it does now (clock, audio, characters). Don't busy-spin while minimised: wait briefly (e.g. a short sleep, or an engine call if one already exists) rather than looping flat out. The ImGui overlay's begin/end must stay paired. docs/engine-guide.md's frame loop says to skip a frame while the framebuffer is 0x0, and the example shows it. Hidden-window paths (captures, bench, self-test) are unaffected: a hidden window keeps its size.

### Fix notes
Root cause: the game's interactive loop and the example's window loop called `render` with `present` set every frame, whatever the framebuffer size, and since cycle 1 `render` throws on a non-positive size. The engine throw stays.

- game/src/main.cpp, interactive loop: after the game update (input, fox asks, clock, foxes, fireball, hero, camera), it reads `window->framebufferSize()` as before. If either dimension is not positive, it updates the audio (volumes, weather, and any new thunder, panned with the last drawn `CameraFrame`), sleeps 16 ms and `continue`s. That skips the brush picks, `render`, the shot, `menu.draw` (which holds both `Overlay::begin()` and `end()`, so they stay paired), the frame counter, the title and `swap`. Events are still polled at the top of the loop.
- examples/minimal/main.cpp: the loop reads `window.framebufferSize()` once. If either dimension is not positive, it sleeps 16 ms and `continue`s, before `frameParams`/`render`/`swap`. Otherwise it passes that size to `frameParams`.
- docs/engine-guide.md: the frame-loop snippet is the example's loop, word for word. A new "Minimised window" step says to skip render, the overlay frame and swap while the size has a zero dimension, keep polling and updating, and wait rather than spin. The intro line says a frame stops after the update while minimised.
- `pxe::Window` has no wait call (no `waitEvents`/`waitEventsTimeout`), so the loops use `std::this_thread::sleep_for`. No engine API was added.

