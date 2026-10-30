## Cycle 1
Build: pass (fresh `cmake --preset check --fresh && cmake --build --preset check --clean-first`; only the stb_image_write `sprintf` warning)

### Changes
- tools/captures.sh: `sleep 1` before the `touch` of the edited file after the edit is applied, and in `cleanup` between the restoring `cp` and its `touch`, so each touch lands in a later whole second than the last compile (make compares timestamps to the whole second). Comment above `cleanup` and the help text's `--edit` entry now say why. No other behaviour changed.
- docs/tasks/captures-edit-stale-build/scripts/verify2.sh, x.cases: the back-to-back Verification 2 sequence (a: terminator edit, b: `--base HEAD --expect same`, c: ripples edit, then the two `cmp`s), `verify2.sh <rounds>`. It uses real newlines in the replacement text (the spec's `\n` would reach python literally).
- tools/README.md: unchanged (its row does not describe the timing).

### Failure before the fix (unfixed tools/captures.sh, 4 rounds back to back)
- Round 2, step b: `t: differs from base` / `captures: 1 cases, 1 checks failed` (the unedited build still contained the terminator edit from step a).
- Round 4, step c: `a == c` (`cmp` of the terminator-edit PNG and the ripples-edit PNG identical: the ripples edit was not compiled).
- Rounds 1 and 3 passed by luck of timing (round 1 also built everything first).

### Verification after the fix
- Verification 2: two runs of 3 rounds each (6 rounds), every step passed, a != c and b != c every round.
- `git status --porcelain`: only `tools/captures.sh` and the task folder; `desert/src/world.cpp` unchanged.

### Issues addressed
- None (no issues.md; spec fix for friction F-045).

### Layering
- Engine: none
- Game: none
- Misplaced code touched: none

### How to playtest
- Not a game change. Run `tools/captures.sh --edit ...` twice back to back as in Verification 2; each run now takes about 2 s longer (two sleeps).

### Docs
- None.

### Known limitations
- Each `--edit` run is 2 s slower. An interrupt during the 1 s sleep in `cleanup` leaves the file restored byte for byte but not touched.
