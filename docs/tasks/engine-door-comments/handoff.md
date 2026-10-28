## Cycle 1
Build: pass

### Changes
- engine/include/pxe/skinned.hpp: `ActorMaterial::crease` comment now reads "a door's planks and grain" instead of "the tavern door's planks and grain".
- engine/shaders/edge.frag: header comment now reads "A character's creased materials (a door's planks and grain) are inked so too, indoors and out;". The paragraph's last two lines were rewrapped to fill the line the shorter wording freed, matching the surrounding wrap width. Comment text only.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: comments only in engine/include/pxe/skinned.hpp and engine/shaders/edge.frag. No types, functions or shader logic changed.
- Game: none.
- Misplaced code touched: none.

### Verification
- `git diff f48dadc -- engine` touches only those three comment lines.
- `grep -rniE "tavern|sleeping fox|\bhero\b|\bfox\b|ruins" engine/include engine/src engine/shaders` returns nothing.
- `cmake --preset check --fresh && cmake --build --preset check --clean-first` succeeds. Its only warning is stb_image_write.h:776 `sprintf` deprecated (third party, pre-existing).

### How to playtest
- Nothing to playtest. The change is to comments only, so rendering is unchanged.

### Known limitations
- None.
