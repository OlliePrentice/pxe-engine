# A capture runner: sets of captures, twice, against a base, under a temporary edit
Type: tooling
Status: queued
Cycle: 0

## Goal
Verification steps keep running sets of captures by hand: each twice and `cmp`-ed for determinism, against a base commit's build, or under a temporary source edit that is applied, built, captured and restored. Copies so far: `docs/tasks/desert-settings-menu/scripts/walk-compare.sh` (cases against the base build), `docs/tasks/perspective-smooth-lighting/scripts/capture-all.sh` (cases into the task folder, each twice), and by hand in `desert-arms`, `desert-arms-detail` (spawn-pose edits), `perspective-sky-clouds` (three edits of the cloud cover) and `desert-lighting` (a no-bounce edit compared with the base; three captures each twice). Friction log F-036, F-038 and F-021. Turn them into one tool, `tools/captures.sh`.

## Decisions
From the retro of the `desert-lighting`/`perspective-sky-clouds` batch, 2026-10-30, under the tooling rule in `.claude/skills/backlog-run/SKILL.md` (step 7).
- **One bash script, `tools/captures.sh`**, in the style of `tools/perspective-check.sh` (`#!/usr/bin/env bash`, `set -euo pipefail`, a `usage` heredoc, macOS and Linux).
- **Cases come from a file**, one per line: `<name> <binary relative to build/release> [flags...]`. Blank lines and lines starting with `#` are skipped. The tool appends `--capture <name>.png` itself. Flags are split on whitespace; no quoting.
- **The base build is the check scripts' own:** `${TMPDIR:-/tmp}/pxe-render-check/<full sha>/`, built the same way and reused, so a base built by `perspective-check.sh` or `ortho-check.sh` serves this tool and the other way round. Copy that block from `perspective-check.sh`; don't change the check scripts.
- **A temporary edit is restored whatever happens:** on success, failure or interrupt (a `trap` on `EXIT`, `INT` and `TERM`), the file is put back byte for byte from a copy and `touch`ed, so the next build recompiles it.
- **Read-mostly:** apart from the edited file while it runs, the tool writes only into its output directory, the base directory and `build/release`.
- **Exit status:** 0 when every case ran (exit 0, PNG written) and every requested check held; 1 when a case failed to run, a build failed or a check failed; 2 on bad usage (unknown option, missing or unreadable cases file, an `--edit` whose old text isn't found exactly once).
- The existing task scripts stay as they are.

## Layering
- Engine: none.
- Game: none.
- Tools: `tools/captures.sh` (new), listed in `tools/README.md`.

## Approach
Read the two scripts named in the Goal and `tools/perspective-check.sh` first.

### Usage (binding)
```
Usage: tools/captures.sh [options] <cases-file>
       tools/captures.sh --help

Runs a set of captures with the working tree's release build (built first).

Cases file: one case per line, <name> <binary relative to build/release>
[flags...]; blank lines and # comments skipped. Each case runs as
<binary> [flags...] --capture <name>.png from the output directory.
  e.g.  d21   desert/pxe_desert --hour 21
        fp    examples/minimal/pxe_minimal --first-person

Options:
  --out <dir>          write <name>.png into <dir> (created if missing).
                       Default: a new ${TMPDIR:-/tmp}/pxe-captures.XXXXXX.
  --twice              run each case again from a scratch directory and cmp:
                       the check is that every case is deterministic.
  --base <ref>         also run each case with <ref>'s build (kept in
                       ${TMPDIR:-/tmp}/pxe-render-check/<sha>/, shared with
                       tools/ortho-check.sh and tools/perspective-check.sh)
                       and cmp; the base PNGs go in <out>/base/.
  --expect same|differ with --base: the check is that every case is
                       identical to (same, the default) or differs from
                       (differ) the base.
  --distinct           the check is that the cases' PNGs are pairwise
                       different.
  --edit <file> <old> <new>
                       before building, replace the one occurrence of the
                       exact text <old> in <file> (relative to the repo root)
                       with <new>; restore the file byte for byte and touch it
                       when the tool exits. Exit 2 if <old> occurs zero or
                       several times.
  -h, --help           print this help.

Exit status: 0 when every case ran and every check held, 1 when a case or a
build failed or a check failed, 2 on bad usage.
```

### Output (binding in content)
One line per case and check, then a summary:
```
outputs: /tmp/pxe-captures.Ab12Cd
edit: desert/src/world.cpp (restored on exit)
d21: written d21.png
d21: deterministic
d21: identical to base
...
captures: 3 cases, all checks held
```
Failing lines read `<name>: NOT deterministic`, `<name>: differs from base` (or `identical to base` under `--expect differ`), `<name> and <other>: identical` under `--distinct`, `<name>: failed (exit <n>)`. The summary is then `captures: <n> cases, <k> checks failed`. Print the base directory as the check scripts do (`base: <sha> in <dir>`).

### The edit
- Do the replacement with a tool that takes `<old>` and `<new>` literally (no regex or sed escapes): e.g. `python3 -I -c` with the strings passed as arguments, or bash parameter expansion after counting occurrences. `<old>` and `<new>` may contain spaces, quotes, `/`, `*` and `&`.
- Copy the file to a temporary backup first. The trap restores it with `cp` and then `touch`es it (CLAUDE.md's shell notes: `cp` keeps the old timestamp). Also `touch` the edited file after the edit, so a build in the same second still recompiles it.
- The edit applies only to the working-tree build. The base build is never edited.

### `tools/README.md`
Add a row after the `perspective-check.sh` row (binding):
```
| `tools/captures.sh [--out dir] [--twice] [--base ref [--expect same\|differ]] [--distinct] [--edit file old new] <cases-file>` | Runs a set of captures listed in a cases file (`<name> <binary under build/release> [flags...]` per line) with the working tree's release build: each twice for determinism (`--twice`), against a base commit's build shared with the check scripts (`--base`), pairwise different (`--distinct`), or under a temporary exact-text edit of one source file, restored and touched on exit (`--edit`). Exit 0 all checks held, 1 a case, build or check failed, 2 bad usage. `--help` for details. |
```

## Verification
Run in the task's worktree. Use `/tmp/captures-check` as scratch (`mkdir -p /tmp/captures-check`) and write this cases file to `/tmp/captures-check/cases.txt`:
```
# test cases
d1630  desert/pxe_desert
d21    desert/pxe_desert --hour 21
fp     examples/minimal/pxe_minimal --first-person
```
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning.
2. `tools/captures.sh --help` exits 0. With no arguments, with `--expect maybe`, and with a missing cases file it exits 2.
3. `tools/captures.sh --twice --distinct --out /tmp/captures-check/a /tmp/captures-check/cases.txt` exits 0, writes the three PNGs, and reports each deterministic.
4. `tools/captures.sh --base HEAD --out /tmp/captures-check/b /tmp/captures-check/cases.txt` exits 0 with all three identical to base; with `--expect differ` added it exits 1.
5. Edit, against the base (binding commands):
   ```
   tools/captures.sh --base HEAD --expect differ --out /tmp/captures-check/c \
     --edit desert/src/world.cpp 'return glm::vec3{r, g, b} / 255.f;' 'return glm::vec3{r, g, b} / 25.f;' \
     /tmp/captures-check/cases.txt
   ```
   (the edit makes the desert's ground bounce ten times brighter in every phase) reports `d1630` and `d21` as differing from base. `fp` (the example) is identical to base, so the tool exits 1 and its summary counts one failed check. Afterwards `git status --porcelain` is empty and `desert/src/world.cpp` is byte-identical to `HEAD` (`git diff --quiet`), and its mtime is newer than before the run.
6. `--edit desert/src/world.cpp 'no such text' 'x'` exits 2 and leaves the file untouched (`git diff --quiet`). An `--edit` whose old text occurs twice (pick a string that appears twice in `desert/src/world.cpp`, e.g. `scene.groundBounce[`) exits 2.
7. Interrupt: start step 5's command in the background, `kill -INT` it once the `edit:` line has printed, wait, and check `git diff --quiet desert/src/world.cpp` succeeds.
8. `cmp /tmp/captures-check/a/d1630.png /tmp/captures-check/b/d1630.png` succeeds (same build, same case).
9. Nothing outside `tools/captures.sh`, `tools/README.md` and the task folder changes: `git diff --stat <cycle base>` lists only those.

## Acceptance criteria
- [ ] `tools/captures.sh` exists, is executable and has the binding usage text.
- [ ] Cases files, `--out`, `--twice`, `--base`, `--expect`, `--distinct` and `--edit` behave as specified, with the binding output lines and exit statuses.
- [ ] An edit is restored byte for byte and touched on success, failure and interrupt.
- [ ] `tools/README.md` has the binding row.
- [ ] Verification 2–9 pass.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Changing `tools/ortho-check.sh` or `tools/perspective-check.sh`.
- Several `--edit`s in one run; edits to the base build.
- Comparing images beyond `cmp` (`tools/png-diff.py`, task `png-diff-tool`).
- Flags on `pxe_desert` for the spawn pose (F-021): `--edit` covers those captures.

## Spec revisions
None.

## Cycle log
