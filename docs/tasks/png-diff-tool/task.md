# A PNG difference reporter
Type: tooling
Status: complete
Cycle: 1

## Goal
Tasks keep writing small scripts to say how two captures differ: `docs/tasks/view-model-edges/scripts/diff_box.py` (the count and bounding box of differing pixels), `docs/tasks/desert-arms-walk/scripts/arm_rows.py` (the box of the difference in each half, to find the arms), and `docs/tasks/desert-lighting/scripts/png-diff.py` (the mean per-channel change over a region), plus a diff by hand in `desert-arms-detail`. Friction log F-027. Turn them into one tool, `tools/png-diff.py`.

## Decisions
From the retro of the `desert-lighting`/`perspective-sky-clouds` batch, 2026-10-30, under the tooling rule in `.claude/skills/backlog-run/SKILL.md` (step 7).
- **One Python file, `tools/png-diff.py`**, run as `python3 -I tools/png-diff.py` or directly (`#!/usr/bin/env python3`, executable). It uses Pillow (`PIL`), as every copy did; it is installed for the system `python3`.
- **Read only.** It never writes an image unless `--mask` asks for one.
- **Exit status:** 0 when the images are identical in the region, 1 when they differ, 2 on bad usage, an unreadable file, or sizes that differ.
- The existing task scripts stay as they are.

## Layering
- Engine: none.
- Game: none.
- Tools: `tools/png-diff.py` (new), listed in `tools/README.md`.

## Approach
Read the three scripts named in the Goal first.

### Usage (binding)
```
usage: png-diff.py [-h] [--region X0,Y0,X1,Y1] [--halves] [--mask OUT.png] a.png b.png

Reports how b.png differs from a.png (same size, compared as RGB): the
number of differing pixels, their bounding box, and the mean change per
channel. Rows count from the top. Exit 0 identical, 1 different, 2 bad usage.

options:
  --region X0,Y0,X1,Y1  compare only columns X0..X1-1 and rows Y0..Y1-1
  --halves              also give the box of the difference in the left and
                        right halves of the region
  --mask OUT.png        write a black image, white where the pixels differ
```

### Output (binding in content)
```
size: 960x540, region 0,0,960,540 (518400 pixels)
differing pixels: 1234 (0.24%)
box: columns 112..845, rows 301..539
left half: columns 112..401, rows 330..539
right half: columns 560..845, rows 301..539
mean change (b - a), RGB 0-255: +0.66 +0.21 -0.02
mean of a, RGB 0-255: 127.0 102.4 72.1
```
- Identical images print `differing pixels: 0` and skip the box lines; the mean lines still print.
- A half with no differing pixel prints `left half: none`.
- The mean change is over every pixel of the region, two decimals with a sign; the mean of `a` one decimal.
- Halves split the region at its middle column: the left half is X0..mid-1, mid = (X0+X1)//2.

## Verification
Run in the task's worktree:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning.
2. `tools/png-diff.py --help` exits 0. With one file, with a missing file, with `--region 1,2,3`, and with two PNGs of different sizes (`docs/tasks/desert-arms-walk/` and `docs/tasks/desert-lighting/` both have 960x540 captures; make a different-sized one with `python3 -I -c "from PIL import Image; Image.new('RGB',(4,4)).save('/tmp/small.png')"`) it exits 2.
3. A file against itself: exit 0, `differing pixels: 0`, mean change `+0.00 +0.00 +0.00`.
4. Against the copies, on the same inputs:
   - `python3 -I docs/tasks/view-model-edges/scripts/diff_box.py A B` and `tools/png-diff.py A B` on `docs/tasks/perspective-smooth-lighting/d1630.png` and `docs/tasks/desert-lighting/d1630.png` give the same count and box (diff_box compares RGBA; the captures are opaque, so the counts agree).
   - `python3 -I docs/tasks/desert-lighting/scripts/png-diff.py A B` on the same pair equals `tools/png-diff.py --region 0,337,960,540 A B`'s mean lines (337 = 540*5//8).
5. `--halves` on that pair prints both halves' lines; `--mask /tmp/m.png` writes a 960x540 image whose white pixel count equals the differing pixel count.
6. `git diff --stat <cycle base>` lists only `tools/png-diff.py`, `tools/README.md` and the task folder.

## Acceptance criteria
- [x] `tools/png-diff.py` exists, is executable and has the binding usage. (git mode 100755; `--help` output diffs empty against the spec's usage block)
- [x] The output lines, `--region`, `--halves` and `--mask` behave as specified, with the exit statuses. (`tools/png-diff.py` `run()`; verification 2–5 outputs in cycle 1 review)
- [x] `tools/README.md` has a row for it after the `glb.py` row: what it reports, its options, its exit statuses, and that it needs Pillow. (row present, all four covered)
- [x] Verification 2–6 pass. (orchestrator rerun: exits 2/2/2/2, self 0 with +0.00s, 61462 px and box columns 0..959 rows 21..539 = diff_box, region means +0.66 +0.21 -0.02 = desert-lighting script, mask 61462 white; diff --stat only the tool, README, task folder)
- [x] Project builds cleanly from a fresh build directory with CMake (check preset, --fresh --clean-first: only the stb_image_write sprintf warning)

## Assets
None.

## Out of scope
- Rendering captures (`tools/captures.sh`, task `capture-tool`); a flag on `pxe_desert` to hide the arms.
- Perceptual or tolerance-based comparison.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base 57494bc.
- Cycle 1 review, 2026-10-30: fresh build clean; verification 2–6 rerun and pass; no issues.

## Playtest checklist
Nothing in either game changed, so there is nothing to play. To try the tool: `tools/png-diff.py --halves --mask /tmp/m.png docs/tasks/perspective-smooth-lighting/d1630.png docs/tasks/desert-lighting/d1630.png` prints 61462 differing pixels with the box and both halves, exits 1, and /tmp/m.png shows the difference in white.
