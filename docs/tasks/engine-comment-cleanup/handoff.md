## Cycle 1
Build: pass

### Changes
- engine/shaders/mesh.frag: actor-lighting comment says "as the scene's stone does" instead of naming the ruins; "the blade" becomes "a blade".
- engine/include/pxe/renderer.hpp: uploadGrass comment no longer names the fox ("say, cleared some blades"); readView comment names RenderConfig::kViewW x kViewH instead of the stale 640x360.
- engine/include/pxe/heightmap.hpp: "a rolling fireball" replaced by "something moving over the ground of the scene".
- engine/src/camera.cpp: initial focus comment describes the 40-pixel offset as an engine default the game overrides with setTarget; the brief/mock/reference-metrics wording is gone. Value unchanged.
- engine/include/pxe/camera.hpp: yaw_ comment "the brief's view" becomes "the default view". Value unchanged.
- engine/include/pxe/lighting.hpp: the firelight comment moved from above the god-ray fields to directly above fireLight/fireReach/fireDim/fireBand. No fields moved, so the struct layout is unchanged.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: comments only, no type or function changes.
- Game: none.
- Misplaced code touched: none.

### How to playtest
- Nothing changes visibly. The game should look and behave exactly as before.

### Known limitations
- None. `grep -rniE "fox|hero|tavern|ruins|fireball|brief" engine/` returns nothing. The build's only warnings are from third_party/stb (sprintf deprecation) and a duplicate-glfw linker note, and both were there before this change.
