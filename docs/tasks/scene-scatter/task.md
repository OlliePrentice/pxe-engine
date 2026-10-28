# Engine grass and leaf scattering
Type: feature
Status: complete
Cycle: 1

## Goal
The second half of moving scene building into the engine (docs/architecture.md: grass and leaf scattering, clearing grass). Move the generic scattering out of game/src/scene.cpp into the engine: Bridson Poisson-disk sampling, scattering grass clumps and tufts over the lawn, dropping grass rooted under stone and bark, and scattering leaf clusters in clumps over tree crowns. The game keeps deciding where things grow and what is cleared for its own objects. The lawn and crowns must come out identical, sprite for sprite.

## Decisions
- RNG order is the contract. Today one `std::mt19937 rng(7)` feeds scatterGrass and then, after the drops and clears (which use no randomness), every leaf draw (scene.cpp buildRuins). The engine functions take the generator by reference from the caller and draw from it in exactly today's order; the game creates it and passes the same one to both. Firefly homes and extra fires keep their own generators in the game.
- Engine capabilities (names the developer's choice, generic):
  - Poisson-disk sampling over a rectangle with an accept predicate (today's `poisson`).
  - Grass scattering over a triangle grid (from scene-geometry): clump spacing, tuft spacing, tufts on/off, and a height field, as parameters. The default height field (`patchiness`) becomes an engine default the caller may pass. The "clumps N, tufts N" log line stays.
  - Dropping grass rooted beneath given surfaces' triangles (today scene.cpp, the stone-and-bark pass), with the surfaces passed in; the "grass sprites under stone or trunks dropped" log may become generic wording but keeps the count.
  - Leaf clusters in clumps over a set of crown triangles at a density (today's clump centres, 1 m clump grid, keep/reject, tilt, standing proud, detail shade), constants unchanged as engine defaults.
- Stays in game: which triangles are lawn and crowns, firefly homes, clearing grass from fire pits and fox beds (game data), extra fires, the "leaf clusters" count log in its current place.
- Bit-identical output: keep every float expression and the order of RNG draws and erases.

## Layering
- Engine: new header(s) and sources for Poisson sampling, grass scattering, dropping grass under surfaces, and leaf scattering. No game content.
- Game: scene.cpp's ruins and hills builders call them in today's order with today's options.

## Acceptance criteria
- [x] `poisson`, `scatterGrass`, `patchiness`, the stone-and-bark drop and the leaf clump scattering no longer live in game/src/scene.cpp; their engine equivalents do. (e4ce436: pxe::poissonDisk, grassPatchiness, scatterGrass, dropGrassUnder, scatterLeaves in engine/src/scatter.cpp; gone from game/src/scene.cpp)
- [x] The same `std::mt19937` instance, seeded 7 in game code, is passed to grass then leaf scattering. (buildRuins: one rng(7) passed to pxe::scatterGrass, then pxe::scatterLeaves)
- [x] Captures are byte-identical to the cycle base, with the same set as scene-geometry: `--hour 15`, `--hour 21`, `--hour 7 --rain 0.3`, `--scene hills`, `--scene hills --stamps`, `--hour 21 --candles --shelter-test --extra-fires 6`, default with `--foxes 3`. Build log counts identical. Record in the handoff. (developer: all seven PNGs and log counts identical; orchestrator: hills, --hour 21 --foxes 3, candles/shelter/extra fires: PNGs and logs identical to the pre-change build)
- [x] `--self-test` passes. (developer and orchestrator: PASS)
- [x] Project builds cleanly from a fresh build directory with CMake. (fresh build-check exit 0)

## Assets
None

## Out of scope
- Any change to where or how densely things grow.

## Cycle log
- Cycle 1: 2026-10-26 started, base 5fcfd91
- Cycle 1: review passed, no issues; complete. Accepted: crown triangle areas computed inside pxe::scatterLeaves (same expression; captures identical)
