#!/usr/bin/env python3
"""Hand-placed pixel sheets for grass, flowers, and leaves.

Wind is a flipbook: each row is a pose amplitude, each column a frame.
Frames lean to the right. The shader mirrors U when the wind faces the
other way, so nothing here is rotated or bent at draw time.

Row 0 of each shape is the calm pose (frames almost identical). Higher
rows lean harder. Rebuild with: python3 tools/sprites/make_sprites.py
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2] / "assets" / "sprites"


def blank(w, h):
    return [[0 for _ in range(w)] for _ in range(h)]


def stamp(buf, cell, ox, oy, pixels, shift):
    cw, ch = cell
    for x, y in pixels:
        # Base stays planted. Only the top of the tuft leans.
        dx = int(round(shift * max(y - 1, 0) / 7.0))
        px = x + dx
        py = y
        if 0 <= px < cw and 0 <= py < ch:
            # buf[0] is the top of the PNG. Art y grows up, so flip inside the cell.
            buf[oy + (ch - 1 - py)][ox + px] = 1


def save_mask(buf, path):
    h = len(buf)
    w = len(buf[0])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            if buf[y][x]:
                # White mask. The engine tints with the instance colour.
                pix[x, y] = (255, 255, 255, 255)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)
    print(f"wrote {path} ({w}x{h})")


# Art sits in a 12x12 cell, y up, x right. The quad pivot is the bottom centre.
GRASS_A = [
    (5, 0), (6, 0),
    (4, 1), (5, 1), (6, 1), (7, 1),
    (4, 2), (7, 2),
    (3, 3), (5, 3), (6, 3), (8, 3),
    (3, 4), (8, 4),
    (4, 5), (7, 5),
    (4, 6), (7, 6),
    (5, 7), (6, 7),
]
GRASS_B = [
    (5, 0), (6, 0),
    (5, 1), (6, 1),
    (4, 2), (6, 2), (7, 2),
    (3, 3), (7, 3),
    (3, 4), (5, 4), (8, 4),
    (2, 5), (4, 5), (8, 5),
    (2, 6), (8, 6),
    (3, 7), (7, 7),
    (4, 8), (6, 8),
    (5, 9),
]
# Calm is still. Frame 6 of band 0 is the rare idle twitch.
GRASS_SHIFT = [
    [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.45, 0.0],
    [0.0, 0.55, 1.15, 1.55, 1.25, 0.70, 0.25, 0.0],
    [0.35, 1.15, 1.90, 2.55, 2.15, 1.35, 0.55, 0.15],
]

FLOWER_A = [
    (5, 0), (6, 0),
    (5, 1), (6, 1),
    (5, 2), (6, 2),
    (5, 3),
    (4, 4), (5, 4), (6, 4), (7, 4),
    (4, 5), (7, 5),
    (5, 6), (6, 6),
]
FLOWER_B = [
    (5, 0), (6, 0),
    (5, 1), (6, 1),
    (5, 2),
    (4, 3), (5, 3), (6, 3), (7, 3),
    (3, 4), (5, 4), (6, 4), (8, 4),
    (4, 5), (5, 5), (6, 5), (7, 5),
    (5, 6), (6, 6),
]
FLOWER_SHIFT = [
    [0.0, 0.0, 0.0, 0.0],
    [0.0, 0.8, 1.4, 0.6],
]

# Leaves: 10x10 cell. Band 0 is a still cluster. Higher bands flutter.
LEAF_A = [
    (3, 3), (4, 3), (5, 3),
    (3, 4), (4, 4), (5, 4), (6, 4),
    (4, 5), (5, 5),
]
LEAF_B = [
    (4, 2), (5, 2),
    (3, 3), (4, 3), (5, 3), (6, 3),
    (3, 4), (5, 4), (6, 4),
    (4, 5), (6, 5),
]
LEAF_SHIFT = [
    [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 0.7, 1.2, 0.8, 0.3, 0.0],
    [0.4, 1.3, 2.1, 1.6, 0.8, 0.2],
]


def build(cell, frames, shapes, shifts, path):
    cw, ch = cell
    rows = len(shapes) * len(shifts)
    buf = blank(frames * cw, rows * ch)
    for s, pixels in enumerate(shapes):
        for band, row_shifts in enumerate(shifts):
            row = s * len(shifts) + band
            # Shader row 0 is the bottom of the texture after the loader flip,
            # so store it at the bottom of the PNG.
            oy = (rows - 1 - row) * ch
            for frame, shift in enumerate(row_shifts):
                stamp(buf, cell, frame * cw, oy, pixels, shift)
    save_mask(buf, path)


def main():
    build((12, 12), 8, [GRASS_A, GRASS_B], GRASS_SHIFT, ROOT / "grass.png")
    build((12, 12), 4, [FLOWER_A, FLOWER_B], FLOWER_SHIFT, ROOT / "flower.png")
    build((10, 10), 6, [LEAF_A, LEAF_B], LEAF_SHIFT, ROOT / "leaf.png")


if __name__ == "__main__":
    main()
