# captures.sh: rebuild the edited file after its edit and after its restore
Type: fix
Status: queued
Cycle: 0

## Goal
`tools/captures.sh --edit` can capture from a stale binary. The presets build with Unix Makefiles, and macOS `make` compares timestamps to the whole second. A desert capture takes under a second, so the restore's `touch` on exit often lands in the same second as the edited file's compile, and the next build (the next `captures.sh` run, or the developer's) skips the file and keeps the edit in the binary. The same happens to an edit applied within a second of the previous restore. Seen in `perspective-surface-detail` (2026-10-30): an unedited `off.png` came out byte-identical to the previous edit's `terminator.png`; `world.cpp` was touched at .605 s and its object compiled at .053 s of the same second. Friction log F-045.

## Decisions
From the retro of the `blender-override-tool`/`desert-sky`/`perspective-surface-detail` batch, 2026-10-30.
- Every `touch` of the edited file in `tools/captures.sh` (after the edit, and on restore) comes in a later whole second than anything built before it, so `make` always sees the file as newer.
- No other behaviour changes: options, output, exit statuses, the restore's byte-for-byte guarantee and the interrupt handling stay as they are.

## Layering
- Engine: none. Game: none.
- Tools: `tools/captures.sh`; `tools/README.md` only if its row describes the timing.

## Approach
In `tools/captures.sh`, before each `touch "$root/$edit_file"` (the apply step and `cleanup`), wait into the next whole second: `sleep 1` is enough and simplest. Update the comment above `cleanup` and the help text's `--edit` line ("restore the file byte for byte and touch it, a second after the build, so the next build recompiles it"). Don't set a future mtime (make warns of clock skew).

## Verification
Run in the task's worktree.
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning.
2. The failing sequence, back to back with no pause between runs. Cases file `x.cases` with one line `t desert/pxe_desert --hour 17.5`; anchor the edits on `  // Nothing is scattered: no grass on the sand, no leaf sprites on the palm.` in `desert/src/world.cpp` (exact text; use the line as it stands if it has changed):
   - `tools/captures.sh --out /tmp/ces/a --edit desert/src/world.cpp "<anchor>" "  scene.terminator[pxe::kSurfSand] = 0.35f;\n<anchor>" x.cases`
   - `tools/captures.sh --out /tmp/ces/b --base HEAD --expect same x.cases`: passes (the unedited build equals the base).
   - `tools/captures.sh --out /tmp/ces/c --edit desert/src/world.cpp "<anchor>" "  scene.ripples.surface = pxe::kSurfSand;\n<anchor>" x.cases`, then `cmp /tmp/ces/a/t.png /tmp/ces/c/t.png` differs and `cmp /tmp/ces/b/t.png /tmp/ces/c/t.png` differs.
   Repeat the whole sequence three times. Every run passes. Before the fix, show the second step failing at least once and record it in the handoff.
3. `git status --porcelain` shows only `tools/captures.sh` (and `tools/README.md` if changed) and the task folder; `desert/src/world.cpp` unchanged.

## Acceptance criteria
- [ ] Every touch of the edited file in `tools/captures.sh` waits into a later second than the last build.
- [ ] Verification 2 passes three times in a row, and the handoff shows the failure before the fix.
- [ ] The help text and comment say why.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Switching the presets to Ninja; the check scripts (they don't edit sources).

## Spec revisions
None.

## Cycle log
