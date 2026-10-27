## ISSUE-001: Minimising the window on Windows makes render throw
Status: open
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
Raised by the developer in handoff.md, cycle 1. Before this task, `Renderer::drawPresent` clamped a zero framebuffer size to 1. Now `Renderer::render` throws `std::invalid_argument` when `present` is set and a size is not positive (engine/src/renderer.cpp, start of `Renderer::render`). GLFW reports a 0x0 framebuffer for a minimised (iconified) window on Windows, so minimising would make the game's interactive loop (game/src/main.cpp) and the example's window loop (examples/minimal/main.cpp) throw and exit.

Required fix (decided in review, recorded in task.md `## Decisions`): the engine keeps the throw. The game's interactive loop and the example's window loop skip drawing while `Window::framebufferSize()` has a zero dimension: no `render`, no overlay frame, no swap, but events are still polled and the game keeps updating as it does now (clock, audio, characters). Don't busy-spin while minimised: wait briefly (e.g. a short sleep, or an engine call if one already exists) rather than looping flat out. The ImGui overlay's begin/end must stay paired. docs/engine-guide.md's frame loop says to skip a frame while the framebuffer is 0x0, and the example shows it. Hidden-window paths (captures, bench, self-test) are unaffected: a hidden window keeps its size.

### Fix notes
