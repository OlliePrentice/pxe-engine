# Fox NPC
Type: feature
Status: complete
Cycle: 2

## Goal
Add a fox to the ruins scene: a skinned, rigged character asset with one looping rest animation, placed at a fixed spot by the campfire when the game loads. It has no AI, movement or interaction. It's set dressing now, and its rig is ready for behaviour later.

## Decisions
- Style follows `docs/art-style.md`, which was written for this task from the hero's look and is now the project standard.
- A stylised red fox. Rust-orange body; cream muzzle, chest, belly and tail tip; dark brown-black lower legs and ear backs; black nose and eyes. Ears and tail slightly oversized for readability at pixel scale.
- Real red-fox proportions (about 0.4 m at the shoulder, about 1 m nose to tail tip), modelled at real size. Drawn in-world at 2.4× (1.5× the hero's 1.6×). The user decided after cycle 1 that the fox should be 1.5× bigger overall; at 1.6× it was only 15–20 px across in the default view. This deliberately overrides `docs/art-style.md`'s 1.6× character scale for the fox; the asset itself stays at real size.
- Rest pose: curled up asleep, nose tucked under the tail, with a slow breathing loop. One clip, `Fox_Rest`, about 4 s, looping seamlessly.
- Full quadruped rig now (spine, neck, head, ears, four legs, tail chain), so walk and run clips can be added later without re-rigging. Only `Fox_Rest` is animated.
- Characters share one actor-material table per frame. The hero uses slots 0–5 and its sword smear uses slot 7 (`hero.cpp` `Hero::pose`). Decision: raise `pxe::kActorMaterials` from 8 to 16, in the engine and in the shader. The hero keeps slots 0–7 unchanged and the fox uses its own slots from 8 upward, so the two characters' colours never couple.
- Pipeline matches the hero: a reproducible Blender script `tools/blender/build_fox.py` writes `assets/characters/fox.glb`.
- Grass is flattened under the fox: when the fox is shown, grass sprites (clumps and tufts) rooted under its curled footprint are dropped, the same way grass under stone and fire pits is dropped, so the curl isn't hidden. User decision after cycle 1. Runs without the fox (captures without `--fox`, `--no-fox`, hills) keep their grass exactly as before.
- Rest pose fixed after cycle 1 (user): the legs must stay attached to the body in the curl, the head lies on the ground (not on the fox's back), and the tail comes round in front of the face and wraps around the body.
- Placed on the lawn near the campfire (campfire at world xz (-5, -6), `scene.cpp` `buildRuins`), curled and facing the fire, on the ground height at that spot. With the larger size, the spot moves out as needed so the whole curl stays at least 0.2 m clear of the fire's stones, logs and dirt patch and of other scene objects; about 1.5–2.2 m from the fire's centre.
- Shown in the ruins scene when playing. In captures it's off unless `--fox` is passed; `--no-fox` hides it when playing. This mirrors `--hero` and `--no-hero`, so existing comparison captures don't change. Never shown in the `hills` scene.
- No collision, no reaction to the hero, sword, rain or fire. It still draws through every pass the hero does: shadows, edges and pond reflection.

## Acceptance criteria
- [x] `tools/blender/build_fox.py` exists, runs headless like `build_hooded_hero.py`, and writes `assets/characters/fox.glb` — `tools/blender/build_fox.py` (headless build with built-in attachment `check()`), writes `assets/characters/fox.glb`
- [x] `assets/characters/fox.glb` contains one skinned mesh, a quadruped skeleton (spine, neck, head, ears, four legs, tail chain) and an animation named `Fox_Rest` — assets.md revision 3: 1 skinned mesh, 27 joints, `Fox_Rest` 4.0 s
- [x] `pxe::kActorMaterials` is 16, and `mesh.frag` (its constant, the material clamp and the `uActorColour`/`uActorGlow` arrays) and `renderer.cpp`'s uniform upload all handle 16 slots; the hero still fills 0–7 as before — `skinned.hpp` kActorMaterials=16; `mesh.frag` kActorMaterials arrays and clamp; `renderer.cpp` upload loops over the constant; `hero.cpp` kCoreSlot=7
- [x] A `game::Fox` class (in `game/src/fox.hpp/.cpp`) loads `fox.glb` through `pxe::SkinnedModel`, throws on a missing file the same way `Hero::build` does, and is caught and logged in `main.cpp` the way the hero is — `fox.cpp` `Fox::build`; `main.cpp` try/catch logging `pxe: no fox:`
- [x] The fox's materials are written into actor-material slots 8 and up, and its skinned vertices' surfaces point at those slots (not 0–7) — `SkinnedModel::load(path, firstMaterial)` with `Fox::kFirstMaterial`=8; `Fox::pose` writes slots 8+
- [x] The fox samples `Fox_Rest` looping, advanced by frame time, and skins at 2.4× scale — `Fox::update`/`Fox::pose`, `Fox::kSize`=2.4
- [x] The fox spawns at a fixed xz near the campfire at (-5, -6), on the ground height there, facing the fire, with its whole curl (at 2.4×) at least 0.2 m clear of the fire's stones, logs and dirt patch and other scene objects — `scene.hpp` `kRuinsFoxSpot` (-6.65,-6.95), 1.9 m from the fire, 0.26 m clear of the dirt patch; `main.cpp` spawns it facing the fire; `Fox::spawn` uses ground height (tilted to the slope)
- [x] When the fox is shown, grass clumps and tufts rooted under its curled footprint are not generated; when it isn't shown, grass generation is unchanged — `scene.cpp` `buildRuins` erase_if on `foxSpot`/`foxRadius`, set in `main.cpp` only when `fox.built()`
- [x] Its mesh is added to `params.actors` in the play loop, so it casts shadows, has edges and reflects like the hero — `main.cpp` `fox.pose(params.actors, ...)` in the capture path and the play loop
- [x] The fox appears in the ruins scene when playing by default, `--no-fox` disables it, captures exclude it unless `--fox` is passed, and it never appears in `hills`; both flags are in the `--help` text — `main.cpp` `foxShown()`, `--fox`/`--no-fox` parsing and `printUsage`
- [x] The fox has no movement, AI, collision or input handling — `fox.hpp/.cpp` have no input, movement or collision
- [x] The README documents the fox and its flags alongside the hero's documentation — README characters paragraph, build-script note, usage line
- [x] Project builds cleanly from a fresh build directory with CMake — orchestrator fresh Debug build, cycle 2: exit 0, only the existing stb/glfw warnings

## Assets
- **fox**: A stylised red fox, curled asleep, in the project's faceted cut-paper style (`docs/art-style.md`). Rust-orange body; cream muzzle, chest, belly and tail tip; dark brown-black lower legs and ear backs; black nose and eyes (closed, asleep). Big bushy tail wrapped round the body, with the nose tucked under it. Ears and tail slightly oversized.
  - Source: `tools/blender/build_fox.py` (reproducible headless build, like `build_hooded_hero.py`)
  - Format: glb (glTF 2.0 binary), at `assets/characters/fox.glb`, preview render at `assets/characters/fox.png`
  - Units and axes: 1 unit = 1 metre, real fox size (about 0.4 m shoulder, about 1 m nose to tail tip when standing); exported +Y up; the fox's head faces -Y in Blender, which is +Z in the engine
  - Origin: ground level, centred under the body
  - Budget: under 3,000 triangles; at most 8 materials, solid colours, no textures; glTF material order is the slot contract (document it)
  - Rig: one armature skinning one mesh; a full quadruped skeleton (root, hips, spine chain, chest, neck, head, ears L/R, four legs with upper/lower/paw, tail chain of at least 4 bones), suitable for future walk/run clips
  - Animations: `Fox_Rest`: curled asleep, slow breathing (chest/spine rise), maybe a slight tail-tip and ear twitch; about 4 s; loops seamlessly
  - Revision 3 (user, after cycle 1): the curl is broken. (1) The hind legs detach from the body when curled; they must stay attached, with no gaps or separated parts at any time in the loop. (2) The head goes onto the fox's back; it must lie on the ground. (3) The tail comes round in front of the fox's face and wraps around the body. Keep joint names, material order, the clip name/length/seamless loop, and real-size modelling (the engine now scales by 2.4). Build in the live session through the MCP, as in revision 2.
  - Revision 2 (user decision after reviewing revision 1): build in the user's live Blender session through the Blender MCP, so the user can watch it go together; the script stays the source of truth. Tighten the curl: head lowered and laid along the body with the nose clearly tucked under the tail (in revision 1 the head sits upright inside the ring, ears up), and remove the clutter visible in the middle of the ring from above. Joint names, material order and clip name stay the same.

## Out of scope
- AI, wandering, waking up, or reacting to the hero, sword, rain, fire or time of day
- Collision with the hero or the scene
- Any clip other than `Fox_Rest`
- Fox in the `hills` scene
- Changes to the hero's look or behaviour (beyond whatever the material table change strictly needs)

## Cycle log
- Cycle 1: 2026-10-21 started
- Cycle 2: 2026-10-21 started. User changes after review: fox 1.5× bigger, grass flattened under it, curl fixed (asset revision 3). Outstanding: ISSUE-001, ISSUE-002
- Cycle 2: 2026-10-21 complete. Fresh build passes; ISSUE-001 and ISSUE-002 resolved.
