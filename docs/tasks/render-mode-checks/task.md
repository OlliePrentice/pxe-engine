# Byte-identical checks per render mode
Type: tooling
Status: queued
Cycle: 0

## Goal
The engine renders in two separate modes, orthographic and perspective (`docs/architecture.md`, "Render modes"). Work on one must never change the other's output. Today the only way to check that is the manual recipe in `CLAUDE.md`: build the base from `git archive`, run each case on both builds, `cmp` the outputs. Turn that recipe into two tools, one per mode: `tools/ortho-check.sh <base-ref>` and `tools/perspective-check.sh <base-ref>`. Each builds the base and the current working tree, runs that mode's standard cases on both, and reports pass or fail per case. Every later task that touches the renderer, shaders or cameras is verified with them, starting with `render-mode-split`.

## Decisions
- **Two tools, one per mode** (user, planning interview Q5). Each is standalone. The build-and-compare logic is duplicated between them rather than put in a shared sourced file, following the project's preference for clear duplicated code over abstraction.
- **Orthographic cases.** All of these are byte-identical cases already listed in `CLAUDE.md`:
  - the game's default capture;
  - `--scene hills`;
  - `--hour 21 --foxes 3`;
  - `--hour 21 --candles --shelter-test --extra-fires 6`;
  - `--self-test`;
  - the example's `--capture`.
- **Perspective cases:**
  - the desert's `--capture`, already standard;
  - the desert's `--hour 21 --capture`, new, so the night phase of the sky and haze is covered;
  - the example's `--first-person --capture`, already standard.
- **The base is built once per commit and kept.** It goes in `${TMPDIR:-/tmp}/pxe-render-check/<full sha>/`, extracted with `git archive` and built with the `release` preset. A later run against the same commit reuses it, so checking several cycles against one base doesn't rebuild it each time.
- **The current side is the working tree as it stands,** uncommitted changes included. It is built with the `release` preset into its own `build/release`, so a worktree checks its own code.
- **The tools print the base build's directory** so a task can run other commands, such as `--bench`, on the same base binaries.
- **Exit status:** 0 when every case matches, 1 when any differs or fails to run, 2 on bad usage.

## Layering
- Engine: none.
- Game: none.
- Tools: `tools/ortho-check.sh` and `tools/perspective-check.sh` (new), listed in `tools/README.md`. `CLAUDE.md`'s byte-identical paragraph points at them.

## Approach
Write `tools/ortho-check.sh` first, then copy it to `tools/perspective-check.sh` and change its case list. Both are bash (`#!/usr/bin/env bash`, `set -euo pipefail`), executable, in the style of `tools/build.sh`. Read `tools/build.sh` first and follow its conventions for usage text and messages.

### Command line (binding)
```
tools/ortho-check.sh <base-ref>
tools/ortho-check.sh --help
```
`<base-ref>` is anything `git rev-parse` accepts (`main`, a hash, `HEAD~1`). Resolve it to the full sha with `git rev-parse --verify "<base-ref>^{commit}"`. A missing or unresolvable ref prints the usage to stderr and exits 2. `--help` prints the usage, the case list and the directories used, then exits 0.

### Steps (binding order)
1. `root=$(git rev-parse --show-toplevel)`, so the tool works from any directory and in any worktree.
2. **Base build.** `base=${TMPDIR:-/tmp}/pxe-render-check/<sha>`. If `$base/build/release/game/pxe` does not exist:
   - `rm -rf "$base"`;
   - `mkdir -p "$base"`;
   - `git -C "$root" archive <sha> | tar -x -C "$base"`;
   - `cmake --preset release -S "$base"`;
   - `cmake --build --preset release` run inside `$base`.

   Print `base: <sha> in $base` either way.
3. **Current build.** `cmake --preset release` then `cmake --build --preset release`, both in `$root`. A failed build on either side exits 1 with the compiler output left on screen.
4. **Output directory.** `out=$(mktemp -d "${TMPDIR:-/tmp}/pxe-render-check-run.XXXXXX")`, with `base/` and `head/` under it. Print `outputs: $out`. Leave it in place afterwards, so a failing case's PNGs can be looked at.
5. **Each case** runs on both sides with the same flags. Run each binary from inside its side's output directory (`cd "$out/base"` or `cd "$out/head"`) and pass a relative PNG name, so the two logs name the same path.
   - A capture case writes `<case>.png`. It passes when both runs exit 0 and `cmp -s` finds the PNGs identical.
   - The `--self-test` case has no PNG. It passes when both runs exit 0 and their combined stdout and stderr are identical.
   - Capture logs aren't compared: the PNG is the output.
6. **Report.** Print one line per case, `PASS <case name>` or `FAIL <case name>: <reason>`. The reason is `png differs`, `log differs`, `base exited <n>` or `head exited <n>`. Then print a summary line, `ortho-check: <passed>/<total> cases identical against <short sha>`, and exit 0 if all passed, else 1.

### Cases (binding)
Case names are what the report prints, and also the PNG file names. `<bin>` is the side's `build/release` directory.

`tools/ortho-check.sh`:
| Case name | Command |
|---|---|
| `game-default` | `<bin>/game/pxe --capture game-default.png` |
| `game-hills` | `<bin>/game/pxe --scene hills --capture game-hills.png` |
| `game-night-foxes` | `<bin>/game/pxe --hour 21 --foxes 3 --capture game-night-foxes.png` |
| `game-night-fires` | `<bin>/game/pxe --hour 21 --candles --shelter-test --extra-fires 6 --capture game-night-fires.png` |
| `game-self-test` | `<bin>/game/pxe --self-test` (log compared) |
| `example-ortho` | `<bin>/examples/minimal/pxe_minimal --capture example-ortho.png` |

`tools/perspective-check.sh`:
| Case name | Command |
|---|---|
| `desert-default` | `<bin>/desert/pxe_desert --capture desert-default.png` |
| `desert-night` | `<bin>/desert/pxe_desert --hour 21 --capture desert-night.png` |
| `example-first-person` | `<bin>/examples/minimal/pxe_minimal --first-person --capture example-first-person.png` |

Pass flags as bash arrays, never as one word-split string. `CLAUDE.md` warns about zsh, and arrays sidestep it in bash too.

`perspective-check.sh` prints `perspective-check:` in its summary line. In step 2 it checks for `$base/build/release/desert/pxe_desert` rather than `game/pxe`: a base from before the desert game has no such binary, and that should rebuild and then fail its cases with `base exited 127`, not silently reuse a partial build. Both tools build everything, so either tool's base build serves the other.

### `tools/README.md`
Add a row per tool to the table. Use the command line above, and say in one line what each does: which mode's standard cases it compares byte-for-byte against a base commit, and where the base build and outputs go.

### `CLAUDE.md`
In "Verifying a change", replace the paragraph that begins "A refactor that should not change output must be **byte-identical**." with:

> A refactor that should not change output must be **byte-identical**. `tools/ortho-check.sh <base>` and `tools/perspective-check.sh <base>` build the base commit in a scratch directory and the working tree, run each render mode's standard cases on both, and compare them (`--help` lists the cases). Run the check for every mode the change could reach. A change to the renderer, the shaders, the cameras or `Scene` and `Lighting` reaches both. Keyboard, mouse and gamepad play is never exercised by these checks.

Keep the sentence after it ("Give the user a playtest checklist for anything that changes it.") as it is. If the paragraph has changed since this spec was written, keep its meaning and only swap the manual recipe and case list for the tools.

## Verification
Run from the repo root, on the task branch:
1. `tools/ortho-check.sh --help` and `tools/perspective-check.sh --help`: both exit 0 and list their cases.
2. `tools/ortho-check.sh` with no argument, and `tools/ortho-check.sh not-a-ref`: both exit 2 with the usage.
3. `tools/ortho-check.sh HEAD`: every case passes, exit 0, and the base directory is printed. This task changes no engine or game code, so HEAD against the working tree is identical. Run it twice: the second run reuses the base build (no `git archive` or full rebuild; it prints the same base directory).
4. `tools/perspective-check.sh HEAD`: every case passes, exit 0.
5. **Negative check.** Temporarily change one colour in `engine/src/scene.cpp`, for example the day `kSurfStone` mid tone. Then:
   - `tools/ortho-check.sh HEAD` must print `FAIL` for at least `game-default` and exit 1;
   - revert the change and confirm `git status` is clean apart from this task's files.

   Do the same with a temporary change to `engine/shaders/backdrop.frag`, for example `kBands = 5.0`: `tools/perspective-check.sh HEAD` must fail `desert-default` and exit 1. Record both runs' summary lines in the handoff.
6. `shellcheck tools/ortho-check.sh tools/perspective-check.sh` if `shellcheck` is installed: no errors. If it isn't installed, say so in the handoff.

## Acceptance criteria
- [ ] `tools/ortho-check.sh` and `tools/perspective-check.sh` exist, are executable, and take exactly `<base-ref>` or `--help`, exiting 2 on bad usage
- [ ] The base is extracted with `git archive` into `${TMPDIR:-/tmp}/pxe-render-check/<full sha>/`, built with the `release` preset, reused when its binaries already exist, and its directory is printed
- [ ] The current side is the working tree's own `release` build
- [ ] `ortho-check.sh` runs exactly the six orthographic cases in the table and `perspective-check.sh` the three perspective cases, with the flags shown, from per-side output directories with relative PNG names
- [ ] Captures are compared with `cmp`, the self-test by its combined output, and each case prints `PASS` or `FAIL <reason>`, followed by a summary line and exit status 0 or 1
- [ ] Verification steps 3 and 4 pass, and the negative checks in step 5 fail as described (summary lines in the handoff)
- [ ] `tools/README.md` lists both tools. `CLAUDE.md`'s byte-identical paragraph points at them in place of the manual recipe
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Any change to engine, game, desert or example code
- Running these checks automatically (CI, git hooks)
- Performance measurement (`--bench`); `render-mode-split` does its own
- Comparing images approximately or showing their differences

## Spec revisions
None.

## Cycle log
