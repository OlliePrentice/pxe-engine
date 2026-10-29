"""Compare two PNGs of the same size: print the number of differing pixels
and their bounding box (first and last column and row, rows from the top).

Usage: python3 -I diff_box.py a.png b.png
"""
import sys

from PIL import Image, ImageChops


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: diff_box.py a.png b.png")
    a = Image.open(sys.argv[1]).convert("RGBA")
    b = Image.open(sys.argv[2]).convert("RGBA")
    if a.size != b.size:
        sys.exit(f"size differs: {a.size} vs {b.size}")
    diff = ImageChops.difference(a, b)
    # Collapse the four channels to one mask: nonzero where any channel differs.
    mask = None
    for ch in diff.split():
        ch = ch.point(lambda v: 255 if v else 0)
        mask = ch if mask is None else ImageChops.lighter(mask, ch)
    count = mask.histogram()[255]
    if count == 0:
        print("differing pixels: 0")
        return
    x0, y0, x1, y1 = mask.getbbox()
    print(f"differing pixels: {count}")
    print(f"columns {x0}..{x1 - 1}, rows {y0}..{y1 - 1}")


main()
