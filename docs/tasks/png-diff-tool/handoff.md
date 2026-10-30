## Cycle 1
Build: pass

### Changes
- tools/png-diff.py (new, executable): reports how b.png differs from a.png as RGB: size/region line, differing pixel count and percentage, bounding box, optional `--halves` boxes, mean change (b - a) and mean of a. `--region`, `--mask` as specified. Pillow only (`Image`, `ImageChops`, `ImageStat`). `--help` prints the binding usage text verbatim. Exit 0 identical, 1 differs, 2 bad usage / unreadable file / size mismatch / bad region / unwritable mask. Mean change is mean(b) - mean(a) via ImageStat (equals the mean of per-pixel differences); `-0.00` is printed as `+0.00`.
- tools/README.md: row after the `glb.py` row.

Choices the spec left open: boxes are in whole-image coordinates even with `--region`; a region must satisfy 0 <= X0 < X1 <= width and 0 <= Y0 < Y1 <= height, else exit 2; `--mask` writes an image the size of the inputs (not of the region), black outside the region, so mask pixels keep their capture coordinates; it is written even when the images are identical.

### Verification
1. `cmake --preset check --fresh && cmake --build --preset check --clean-first`: exit 0, only the stb_image_write `sprintf` warning.
2. Exit statuses (A = docs/tasks/perspective-smooth-lighting/d1630.png, B = docs/tasks/desert-lighting/d1630.png):
```
--help                       -> help text, exit 0
png-diff.py A                -> png-diff.py: the following arguments are required: b   (exit 2)
png-diff.py A /tmp/nope.png  -> png-diff.py: cannot read /tmp/nope.png: [Errno 2] No such file or directory: '/tmp/nope.png'   (exit 2)
--region 1,2,3 A B           -> png-diff.py: --region wants four comma-separated integers X0,Y0,X1,Y1, got '1,2,3'   (exit 2)
A /tmp/small.png (4x4)       -> png-diff.py: size differs: ...d1630.png is 960x540, /tmp/small.png is 4x4   (exit 2)
```
3. `tools/png-diff.py A A` (exit 0):
```
size: 960x540, region 0,0,960,540 (518400 pixels)
differing pixels: 0 (0.00%)
mean change (b - a), RGB 0-255: +0.00 +0.00 +0.00
mean of a, RGB 0-255: 151.8 144.4 130.6
```
4. diff_box.py A B:
```
differing pixels: 61462
columns 0..959, rows 21..539
```
`tools/png-diff.py A B` (exit 1):
```
size: 960x540, region 0,0,960,540 (518400 pixels)
differing pixels: 61462 (11.86%)
box: columns 0..959, rows 21..539
mean change (b - a), RGB 0-255: +0.34 +0.12 -0.02
mean of a, RGB 0-255: 151.8 144.4 130.6
```
Same count and box. The desert-lighting png-diff.py A B printed `[0.66, 0.21, -0.02] mean old [126.6, 101.8, 71.9]` (plus a Pillow getdata DeprecationWarning); `tools/png-diff.py --region 0,337,960,540 A B` tail:
```
mean change (b - a), RGB 0-255: +0.66 +0.21 -0.02
mean of a, RGB 0-255: 126.6 101.8 71.9
```
Equal.
5. `tools/png-diff.py --halves --mask /tmp/m.png A B`:
```
size: 960x540, region 0,0,960,540 (518400 pixels)
differing pixels: 61462 (11.86%)
box: columns 0..959, rows 21..539
left half: columns 0..479, rows 41..539
right half: columns 480..959, rows 21..539
mean change (b - a), RGB 0-255: +0.34 +0.12 -0.02
mean of a, RGB 0-255: 151.8 144.4 130.6
```
Mask: size (960, 540), white pixels 61462 = differing pixel count. Also checked `--region 100,300,301,500 --halves` against a brute-force pixel loop: counts and boxes of whole, left (cols 100..199, 3957 px) and right (cols 200..300, 8728 px) match.
6. `git diff --stat` against 57494bc lists only tools/png-diff.py, tools/README.md and the task folder.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none
- Game: none
- Misplaced code touched: none

### How to playtest
- Nothing in-game. Run `tools/png-diff.py a.png b.png`.

### Docs
- None beyond the `tools/README.md` row already added.

### Known limitations
- Only the stated edge cases were exercised; PNGs with alpha are compared as RGB after conversion (alpha ignored), as specified.
