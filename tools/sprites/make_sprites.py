#!/usr/bin/env python3
"""Pixel sheets for grass, flowers, and leaves.

Each tuft is a fan of 1px blades, one flat colour (the engine tints it).
Columns are a 4-frame flipbook held at 2 fps. Rows are shape, then lean:
calm, breezy, storm. The shader mirrors U when the wind faces the other
way, so every frame here leans to the right. Row 0 is the bottom of the
PNG, because the loader flips the texture and the quad's v=0 is the base.

Rebuild with: python3 tools/sprites/make_sprites.py
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2] / "assets" / "sprites"


def blank(w, h):
    return [[0 for _ in range(w)] for _ in range(h)]


def lean(pixels, amount, height):
    out = []
    for x, y in pixels:
        dx = int(round(amount * y / height))
        out.append((x + dx, y))
    return out


def stamp(buf, cell, ox, oy, pixels):
    cw, ch = cell
    for x, y in pixels:
        if 0 <= x < cw and 0 <= y < ch:
            buf[oy + (ch - 1 - y)][ox + x] = 1


def save_mask(buf, path):
    h = len(buf)
    w = len(buf[0])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            if buf[y][x]:
                pix[x, y] = (255, 255, 255, 255)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)
    print(f"wrote {path} ({w}x{h})")


def build(cell, shapes, shifts, path):
    cw, ch = cell
    frames = len(shifts[0])
    rows = len(shapes) * len(shifts)
    buf = blank(frames * cw, rows * ch)
    for s, pixels in enumerate(shapes):
        for band, row_shifts in enumerate(shifts):
            row = s * len(shifts) + band
            oy = (rows - 1 - row) * ch
            for frame, amount in enumerate(row_shifts):
                stamp(buf, cell, frame * cw, oy, lean(pixels, amount, ch - 3))
    save_mask(buf, path)


# 12x12 cell, y up. Four blades from a shared base. Drawn inside x=1..8
# so the strongest lean (about +2.5px at the tip) still fits.
GRASS_A = [
    (4, 0), (3, 1), (3, 2), (2, 3), (2, 4), (2, 5),
    (5, 0), (5, 1), (4, 2), (4, 3), (4, 4), (3, 5), (3, 6), (3, 7), (3, 8),
    (6, 0), (6, 1), (6, 2), (7, 3), (7, 4), (7, 5), (7, 6), (8, 7),
    (7, 0), (8, 1), (8, 2), (9, 3), (9, 4), (9, 5),
]
# Three blades, wider and a little shorter.
GRASS_B = [
    (4, 0), (3, 1), (3, 2), (2, 3), (2, 4), (1, 5), (1, 6), (1, 7),
    (5, 0), (5, 1), (5, 2), (6, 3), (6, 4), (6, 5), (7, 6), (7, 7), (7, 8),
    (6, 0), (7, 1), (7, 2), (8, 3), (8, 4), (9, 5),
]

# Frame 2 is the far lean. Frame 3 comes back, so the loop does not pop.
GRASS_SHIFT = [
    [0.0, 0.35, 0.70, 0.30],
    [0.15, 1.05, 1.85, 0.90],
    [0.45, 1.55, 2.55, 1.20],
]

# 8x8 cell. Purple X sits low, on the ground. Yellow is a 2px dot.
FLOWER_A = [
    (2, 1), (3, 2), (4, 3), (5, 4),
    (5, 1), (4, 2), (3, 3), (2, 4),
    (2, 2), (5, 2), (3, 3), (4, 3),
]
FLOWER_B = [
    (3, 1), (4, 1),
    (3, 2), (4, 2),
]
FLOWER_SHIFT = [
    [0.0, 0.25, 0.55, 0.20],
    [0.10, 0.70, 1.25, 0.55],
]

# 10x10 cell. Small overlapping clusters, not a filled oval.
LEAF_A = [
    (4, 3), (5, 3), (6, 3),
    (4, 4), (5, 4),
    (5, 5), (6, 5),
    (5, 6),
]
LEAF_B = [
    (3, 3), (4, 3), (5, 3), (6, 3),
    (4, 4), (5, 4), (6, 4),
    (5, 5), (6, 5),
    (6, 6),
]
LEAF_SHIFT = [
    [0.0, 0.30, 0.60, 0.25],
    [0.10, 0.80, 1.40, 0.70],
    [0.30, 1.20, 2.00, 1.00],
]


def main():
    build((12, 12), [GRASS_A, GRASS_B], GRASS_SHIFT, ROOT / "grass.png")
    build((8, 8), [FLOWER_A, FLOWER_B], FLOWER_SHIFT, ROOT / "flower.png")
    build((10, 10), [LEAF_A, LEAF_B], LEAF_SHIFT, ROOT / "leaf.png")


if __name__ == "__main__":
    main()
