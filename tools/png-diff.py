#!/usr/bin/env python3
"""Reports how b.png differs from a.png. Run `png-diff.py --help` for details.

Needs Pillow. Read only: the only file it writes is the one --mask names.
Exit 0 identical in the region, 1 different, 2 bad usage, unreadable file or
sizes that differ.
"""
import argparse
import sys

from PIL import Image, ImageChops, ImageStat

HELP = """\
usage: png-diff.py [-h] [--region X0,Y0,X1,Y1] [--halves] [--mask OUT.png] a.png b.png

Reports how b.png differs from a.png (same size, compared as RGB): the
number of differing pixels, their bounding box, and the mean change per
channel. Rows count from the top. Exit 0 identical, 1 different, 2 bad usage.

options:
  --region X0,Y0,X1,Y1  compare only columns X0..X1-1 and rows Y0..Y1-1
  --halves              also give the box of the difference in the left and
                        right halves of the region
  --mask OUT.png        write a black image, white where the pixels differ
"""


class Usage(Exception):
    pass


class Parser(argparse.ArgumentParser):
    def error(self, message):
        raise Usage(message)


class Help(argparse.Action):
    def __init__(self, option_strings, dest, **kwargs):
        super().__init__(option_strings, dest, nargs=0, default=argparse.SUPPRESS, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        sys.stdout.write(HELP)
        sys.exit(0)


def make_parser():
    p = Parser(prog="png-diff.py", add_help=False)
    p.add_argument("-h", "--help", action=Help)
    p.add_argument("--region", metavar="X0,Y0,X1,Y1")
    p.add_argument("--halves", action="store_true")
    p.add_argument("--mask", metavar="OUT.png")
    p.add_argument("a")
    p.add_argument("b")
    return p


def parse_region(text, size):
    try:
        x0, y0, x1, y1 = (int(v) for v in text.split(","))
    except ValueError:
        raise Usage(f"--region wants four comma-separated integers X0,Y0,X1,Y1, got '{text}'")
    w, h = size
    if not (0 <= x0 < x1 <= w and 0 <= y0 < y1 <= h):
        raise Usage(f"--region {text} is not inside the {w}x{h} image (need 0 <= X0 < X1 <= {w}, 0 <= Y0 < Y1 <= {h})")
    return x0, y0, x1, y1


def load(path):
    try:
        return Image.open(path).convert("RGB")
    except (OSError, ValueError) as e:
        raise Usage(f"cannot read {path}: {e}")


def difference_mask(a, b):
    """L image, 255 where any channel of a and b differs, else 0."""
    mask = None
    for ch in ImageChops.difference(a, b).split():
        ch = ch.point(lambda v: 255 if v else 0)
        mask = ch if mask is None else ImageChops.lighter(mask, ch)
    return mask


def box_text(mask, x0, y0):
    """'columns C0..C1, rows R0..R1' of the mask's white pixels in image
    coordinates (the mask's corner is at x0,y0), or 'none'."""
    bbox = mask.getbbox() if mask.width > 0 and mask.height > 0 else None
    if bbox is None:
        return "none"
    bx0, by0, bx1, by1 = bbox
    return f"columns {x0 + bx0}..{x0 + bx1 - 1}, rows {y0 + by0}..{y0 + by1 - 1}"


def signed(v):
    s = f"{v:+.2f}"
    return "+0.00" if s == "-0.00" else s


def run(argv):
    args = make_parser().parse_args(argv)
    a, b = load(args.a), load(args.b)
    if a.size != b.size:
        raise Usage(f"size differs: {args.a} is {a.size[0]}x{a.size[1]}, {args.b} is {b.size[0]}x{b.size[1]}")
    w, h = a.size
    x0, y0, x1, y1 = parse_region(args.region, a.size) if args.region else (0, 0, w, h)
    ra, rb = a.crop((x0, y0, x1, y1)), b.crop((x0, y0, x1, y1))
    mask = difference_mask(ra, rb)
    count = mask.histogram()[255]
    pixels = (x1 - x0) * (y1 - y0)

    print(f"size: {w}x{h}, region {x0},{y0},{x1},{y1} ({pixels} pixels)")
    print(f"differing pixels: {count} ({100.0 * count / pixels:.2f}%)")
    if count:
        print(f"box: {box_text(mask, x0, y0)}")
        if args.halves:
            mid = (x0 + x1) // 2
            print(f"left half: {box_text(mask.crop((0, 0, mid - x0, y1 - y0)), x0, y0)}")
            print(f"right half: {box_text(mask.crop((mid - x0, 0, x1 - x0, y1 - y0)), mid, y0)}")
    mean_a = ImageStat.Stat(ra).mean
    mean_b = ImageStat.Stat(rb).mean
    print("mean change (b - a), RGB 0-255: " + " ".join(signed(mb - ma) for ma, mb in zip(mean_a, mean_b)))
    print("mean of a, RGB 0-255: " + " ".join(f"{m:.1f}" for m in mean_a))

    if args.mask:
        # Same size as the inputs, so a pixel keeps its coordinates; black outside the region.
        out = Image.new("L", (w, h), 0)
        out.paste(mask, (x0, y0))
        try:
            out.save(args.mask, format="PNG")
        except (OSError, ValueError) as e:
            raise Usage(f"cannot write {args.mask}: {e}")
    return 1 if count else 0


def main():
    try:
        return run(sys.argv[1:])
    except Usage as e:
        sys.stderr.write(f"png-diff.py: {e}\n")
        return 2


if __name__ == "__main__":
    sys.exit(main())
