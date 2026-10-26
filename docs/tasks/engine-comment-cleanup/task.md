# Engine comment cleanup
Type: fix
Status: open
Cycle: 1

## Goal
Engine comments name game content, which docs/architecture.md forbids ("No game content in the engine ... in engine identifiers, APIs or comments"), and two engine comments are stale. Reword them in generic terms. Comments only: no code, shader logic, value or behaviour changes.

## Decisions
- Comment-only change. Generated code and rendered output must be unchanged.
- The camera's initial focus offset and 45-degree yaw stay as engine defaults; only their comments change.

## Layering
- Engine: comment rewording in engine/ only.
- Game: none.

## Reproduction
- Observed: these engine comments name game content or are wrong:
  - engine/shaders/mesh.frag:15-19: "darken and cool with the evening as the ruins do".
  - engine/include/pxe/renderer.hpp:129-130: "blades cleared from under a fox added as it runs".
  - engine/include/pxe/heightmap.hpp:11: "a rolling fireball".
  - engine/src/camera.cpp:54-55: "matching the brief's mock so the hill and the lit stripes sit where the reference metrics expect".
  - engine/include/pxe/camera.hpp:64: "the brief's view".
  - engine/include/pxe/renderer.hpp:140: "Centre 640×360", but the view is RenderConfig::kViewW x kViewH (960×540, engine/include/pxe/config.hpp).
  - engine/include/pxe/lighting.hpp:37-41: the firelight comment sits above the god-ray fields instead of the firelight fields it describes.
- Expected: each comment describes the capability in generic terms, with no game content, and is accurate.

## Acceptance criteria
- [ ] Each comment listed under Reproduction is reworded in generic terms (e.g. "as the scene's stone does", "after the game has cleared some", "something moving over the ground").
- [ ] readView's comment states the size correctly, preferably by naming RenderConfig::kViewW x kViewH rather than a number that can go stale.
- [ ] The firelight comment in lighting.hpp sits directly above the firelight fields, and the god-ray comment above the god-ray fields.
- [ ] `grep -rniE "fox|hero|tavern|ruins|fireball|brief" engine/` finds nothing that names game content (credits to t3ssel8r and generic nouns are fine).
- [ ] The diff touches only comments in engine/.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None

## Out of scope
- Any code, shader or value change.
- Comments in game/.

## Cycle log
- Cycle 1: 2026-10-26 started, base 14e2e12
