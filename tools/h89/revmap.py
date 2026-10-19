#!/usr/bin/env python3
"""Which cells of an H19 capture are reverse video: an 8x10-cell, 640x500
PNG from h89_headless --ppm (converted). A reverse cell is mostly lit; a
normal one mostly dark. Prints one line of '#' (reverse) and '.' per row.

  revmap.py SHOT.png [ROWS]       ROWS like 0-3,22-24 (default all 25)
"""
import sys
from PIL import Image

img = Image.open(sys.argv[1]).convert("L")
W, H = img.size
cw, ch = W // 80, H // 25
rows = range(25)
if len(sys.argv) > 2:
    rows = []
    for part in sys.argv[2].split(","):
        a, _, b = part.partition("-")
        rows += range(int(a), int(b or a) + 1)
px = img.load()
for r in rows:
    line = ""
    for c in range(80):
        lit = sum(1 for y in range(r * ch, (r + 1) * ch)
                  for x in range(c * cw, (c + 1) * cw) if px[x, y] > 128)
        line += "#" if lit > (cw * ch) // 2 else "."
    print("%2d %s" % (r, line.rstrip(".")))
