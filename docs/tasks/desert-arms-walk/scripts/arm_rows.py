"""Where the arms are in each capture: diff it against the same capture rendered without the arms.
usage: arm_rows.py <with.png> <without.png> [<with.png> <without.png> ...]
Prints the topmost row with an arm pixel, the pixel count, and each half's rows and columns."""
import sys
from PIL import Image
a = sys.argv[1:]
for w, n in zip(a[0::2], a[1::2]):
    A, B = Image.open(w).convert("RGB"), Image.open(n).convert("RGB")
    W, H = A.size
    pa, pb = A.load(), B.load()
    pts = [(x, y) for y in range(H) for x in range(W) if pa[x, y] != pb[x, y]]
    out = [w.split("/")[-1], f"top row {min(y for x, y in pts)}", f"pixels {len(pts)}"]
    for name, sel in (("left", [p for p in pts if p[0] < W // 2]), ("right", [p for p in pts if p[0] >= W // 2])):
        if sel:
            out.append(f"{name}: rows {min(y for x, y in sel)}-{max(y for x, y in sel)}, cols {min(x for x, y in sel)}-{max(x for x, y in sel)}")
    print("; ".join(out))
