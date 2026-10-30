## Cycle 1
Build: pass

### Changes
- tools/captures.sh (new, executable): the capture runner. Parses options and the cases file (exit 2 on bad usage, including an `--edit` whose old text isn't found exactly once, checked before any build), builds the base as the check scripts do (block copied from `tools/perspective-check.sh`, same `${TMPDIR:-/tmp}/pxe-render-check/<sha>/`), applies the optional edit (python3 -I, bytes, literal strings; backup copy first; `touch` after the edit), builds the working tree with the release preset, runs each case from the output directory, and runs the `--twice`, `--base`/`--expect` and `--distinct` checks. A `trap` on EXIT, INT and TERM restores the file with `cp` from the backup and touches it. Usage text is the binding text.
- tools/README.md: the binding row after the `perspective-check.sh` row.
- docs/tasks/capture-tool/scripts/interrupt-test.py: helper for verification 7 (see Known limitations).

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none
- Game: none (tools only)
- Misplaced code touched: none

### Verification results (worktree, scratch in /tmp/captures-check)
1. `cmake --preset check --fresh && cmake --build --preset check --clean-first`: rc 0, only the stb_image_write `sprintf` warning.
2. `--help` exit 0 and its output equals the binding usage text; no arguments, `--expect maybe`, missing cases file: exit 2 (also unknown option, `--expect` without `--base`).
3. `--twice --distinct --out a`: exit 0, three PNGs, each "deterministic", "distinct: every pair of PNGs differs".
4. `--base HEAD --out b`: exit 0, all three "identical to base"; with `--expect differ`: exit 1 ("3 checks failed").
5. Binding edit command: `d1630` and `d21` "differs from base", `fp` "identical to base", exit 1, "captures: 3 cases, 1 checks failed". Afterwards `git status --porcelain desert` empty, `git diff --quiet desert/src/world.cpp` succeeds, mtime newer than before the run.
6. `--edit ... 'no such text'`: exit 2, file untouched. `scene.groundBounce[` (4 occurrences): exit 2, file untouched.
7. Interrupt: see Known limitations for how it was run. With the signal delivered, SIGINT gives exit 130 and SIGTERM exit 143, the file is restored and touched in both.
8. `cmp a/d1630.png b/d1630.png` succeeds.
9. `git diff --stat 010f23d` lists `tools/README.md` and the orchestrator's `task.md` start-of-cycle change; `tools/captures.sh` and the task folder's new files are untracked until this commit. Nothing under `desert/`, `engine/` or `game/` changed.

### How to playtest
- Not a game change. Try: `tools/captures.sh --twice --distinct cases.txt` with a cases file such as the one in the task's Verification, or `--base main --expect same` to check a refactor, or `--edit <file> '<old>' '<new>' --base HEAD --expect differ` to test a temporary source change. `--help` for details.

### Docs
- tools/README.md row is already added (binding row). Nothing else.

### Known limitations
- Verification 7 as worded (`cmd &` then `kill -INT`) does not work from a non-interactive shell: a background job there starts with SIGINT ignored, and bash cannot trap a signal that was ignored on entry, so the tool runs to completion (and restores the file normally on exit). I ran the interrupt check with `docs/tasks/capture-tool/scripts/interrupt-test.py <repo> [INT|TERM]`, which starts the tool with the default signal disposition. Ctrl-C in a terminal and `kill -INT`/`-TERM` on a normally started tool work.
- A signal sent only to the script while cmake runs is handled when the foreground cmake child exits (bash defers traps), so the restore happens at the end of the current build; Ctrl-C in a terminal also reaches the child, so there it is immediate.
- Case names are limited to letters, digits, `.`, `_` and `-`, and must be unique (exit 2 otherwise); the spec didn't say, and the name becomes a file name.
- Per-case logs (`<name>.log`, and in `base/` and the scratch directory) are written next to the PNGs for diagnosing a failed case.
- Summary reads "1 checks failed" for one failure (the spec's wording).

## Cycle 2
Build: pass

### Changes
- tools/captures.sh: "Temporary edit" copies the file into `backup_tmp` and sets `edit_backup` (which arms the EXIT-trap restore) only after the `cp` has finished, before the edit is applied. Nothing else changed.
- docs/tasks/capture-tool/issues.md: ISSUE-001 set to fixed with fix notes.

### Issues addressed
- ISSUE-001: the restore is armed only once the backup is complete.

### Layering
- Engine: none
- Game: none
- Misplaced code touched: none

### How to playtest
- Not a runtime feature. Verification re-run after the fix (all in the worktree, `--base HEAD`, cases file from the spec):
  - Verification 5 (edit against base, `--expect differ`): `d1630` and `d21` differ from base, `fp` identical, summary "3 cases, 1 checks failed", exit 1. Afterwards `git status --porcelain` shows only `tools/captures.sh` (the uncommitted fix), `desert/src/world.cpp` passes `git diff --quiet`, and its mtime moved from 1793372328 to 1793372409.
  - Verification 6: `--edit ... 'no such text' 'x'` exits 2, file clean; `--edit ... 'scene.groundBounce['` (4 occurrences) exits 2, file clean.
  - Interrupt check, `scripts/interrupt-test.py` with INT (exit status 130) and TERM (143): edited while running True, restored True, mtime newer True, `desert/src/world.cpp` clean afterwards.
  - `bash -n tools/captures.sh` passes. The base directory `${TMPDIR}/pxe-render-check/06c765a...` was built for this run and kept.
- Verification 1-4, 7-9 were not re-run: the change touches only the edit block, which none of them reach (no `--edit`) beyond 7, which the interrupt script covers.

### Docs
- None.

### Known limitations
- An interrupt between `mktemp` and the end of `cp` leaves a stray `pxe-captures-edit.*` file in the temp directory (by design, harmless).
