# Mean per-channel difference (new - old) over the lower half (ground) of two PNGs.
# Usage: python3 -I png-diff.py old.png new.png
import sys
from PIL import Image
a = Image.open(sys.argv[1]).convert("RGB"); b = Image.open(sys.argv[2]).convert("RGB")
w, h = a.size
pa = list(a.crop((0, h * 5 // 8, w, h)).getdata()); pb = list(b.crop((0, h * 5 // 8, w, h)).getdata())
n = len(pa)
print([round(sum(q[c] - p[c] for p, q in zip(pa, pb)) / n, 2) for c in range(3)],
      "mean old", [round(sum(p[c] for p in pa) / n, 1) for c in range(3)])
