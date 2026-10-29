## Cycle 1
Build: pass

### Changes
- tools/ortho-check.sh (new, executable): `<base-ref>` or `--help`. Resolves the ref with `git rev-parse --verify`, builds the base from `git archive` in `${TMPDIR:-/tmp}/pxe-render-check/<full sha>/` with the release preset (reused when `build/release/game/pxe` exists), builds the working tree's own `build/release`, runs the six orthographic cases from `<run dir>/base` and `<run dir>/head` with relative PNG names, prints PASS/FAIL per case and a summary line. Exit 0/1/2. Cases are run through a small `run_case` function taking the flags as separate arguments (no word-split strings).
- tools/perspective-check.sh (new, executable): the same script with the three perspective cases, `perspective-check:` summary, and the base-reuse sentinel `build/release/desert/pxe_desert`.
- tools/README.md: one row per tool.
- CLAUDE.md: the byte-identical paragraph replaced as the spec gives. The spec's replacement ends with the "Keyboard, mouse and gamepad..." sentence, which was a separate paragraph before; the new text is one paragraph, with "Give the user a playtest checklist..." kept after it unchanged.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none
- Game: none
- Misplaced code touched: none

### Verification
1. `--help` on both: exit 0, cases listed.
2. No argument, and `not-a-ref`: both exit 2 with usage (also two arguments: exit 2).
3. `tools/ortho-check.sh HEAD` (first run, full base build, ~47 s): `ortho-check: 6/6 cases identical against b476fe0`, exit 0. It printed `base: b476fe08c78b252ab040316f0c3a09e1896405c8 in /var/folders/x2/nc5m77px2vj6cms3s47ygw_m0000gn/T//pxe-render-check/b476fe08c78b252ab040316f0c3a09e1896405c8`. Second run: same base directory, no archive or rebuild of the base (~10 s total), 6/6, exit 0.
4. `tools/perspective-check.sh HEAD`: `perspective-check: 3/3 cases identical against b476fe0`, exit 0 (reused the base from step 3).
5. Negative checks (both reverted with `git checkout`, `git status` shows only this task's files afterwards):
   - `kSurfStone` day mid tone `0xa49c8c` -> `0xa49c8d` in `engine/src/scene.cpp`: `ortho-check: 2/6 cases identical against b476fe0`, exit 1. FAIL game-default, game-night-foxes, game-night-fires, example-ortho (png differs); game-hills and game-self-test passed.
   - `kBands = 6.0` -> `5.0` in `engine/shaders/backdrop.frag`: `perspective-check: 0/3 cases identical against b476fe0`, exit 1. FAIL desert-default, desert-night, example-first-person (png differs).
   - After the reverts `tools/ortho-check.sh HEAD` was 6/6 again, exit 0.
6. shellcheck: not installed on this machine, so not run. `bash -n` passes on both scripts.

### How to playtest
- Not a gameplay change. Run `tools/ortho-check.sh main` / `tools/perspective-check.sh main` from any worktree.

### Docs
- tools/README.md and CLAUDE.md are updated in this task. docs/architecture.md and docs/engine-guide.md need nothing. Note: the `tools/build.sh` row in tools/README.md still says `[game|example]` and omits `desert` (existing, not touched).

### Known limitations
- Tools build the working tree with the `release` preset into `build/release`, as specified. Exit status on a build failure is 1, as specified.
- A base commit from before the desert game would rebuild and then fail with `base exited 127` in perspective-check, as specified. Not exercised here.
- shellcheck was not available.
