#!/usr/bin/env python3
"""
Render the CoCo's 32x16 page from the 512 bytes of it.

The 6847 decides per byte whether a cell is a character or a 2x2 block of
colour, so a decode has to say which, and for a semigraphics cell it has to say
both the colour and which quadrants are lit. That is not decoration: the Gmail
mark and the unread chips down column 0 are the parts of this client that only
exist on this machine, and neither is checkable from a picture of text.

Three panes come out:

  text        the glyphs, with inverse video marked
  video       '#' where a cell is inverse -- the selection bar, the header
  colour      one letter per semigraphics cell, '.' for text

and then the quadrant detail, which is the screen drawn at 64x32 with each
semigraphics cell expanded into its four blocks. That last pane is where the
mark's four strokes and the buff envelope behind them are legible, and it is
drawn in exactly the form src/coco/logo.c comments its byte tables in -- so a
capture can be compared against the source picture character for character.

Reads the hex on stdin or from a file named on the command line.
"""

import sys

# The 6847's 64 glyphs, in (byte & 0x3F) order.
FONT = ("@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]↑←"
        " !\"#$%&'()*+,-./0123456789:;<=>?")

# Semigraphics colours, bits 6-4.
SG_NAME = "GYBRWCMO"
SG_FULL = {"G": "green", "Y": "yellow", "B": "blue", "R": "red",
           "W": "buff", "C": "cyan", "M": "magenta", "O": "orange"}

COLS, ROWS = 32, 16


def panes(mem):
    text, video, colour = [], [], []

    for r in range(ROWS):
        t, v, c = "", "", ""
        for x in range(COLS):
            b = mem[r * COLS + x]
            if b >= 0x80:
                t += " "
                v += " "
                c += SG_NAME[(b >> 4) & 7] if (b & 0x0F) else "-"
            else:
                t += FONT[b & 0x3F]
                # Bit 6 SET is normal video on this machine; clear is inverse.
                v += " " if (b & 0x40) else "#"
                c += "."
        text.append(t)
        video.append(v)
        colour.append(c)

    return text, video, colour


def quadrants(mem):
    """The screen at 64x32: each semigraphics cell as its four blocks."""
    out = []
    for r in range(ROWS):
        top, bot = "", ""
        for x in range(COLS):
            b = mem[r * COLS + x]
            if b >= 0x80:
                n = SG_NAME[(b >> 4) & 7]
                top += (n if b & 0x08 else ".") + (n if b & 0x04 else ".")
                bot += (n if b & 0x02 else ".") + (n if b & 0x01 else ".")
            else:
                ch = FONT[b & 0x3F]
                # A character occupies the whole cell; show it once, upper half.
                top += ch + " "
                bot += "  "
        out.append(top)
        out.append(bot)
    return out


def rule(label):
    return f"--- {label} " + "-" * (COLS - len(label) - 5)


def main():
    src = open(sys.argv[1]) if len(sys.argv) > 1 else sys.stdin
    mem = bytes.fromhex(src.read().strip())
    if len(mem) < COLS * ROWS:
        sys.exit(f"need {COLS * ROWS} bytes, got {len(mem)}")

    text, video, colour = panes(mem)

    print(rule("text"))
    print("    " + "".join(str(i % 10) for i in range(COLS)))
    for r in range(ROWS):
        print(f"{r:2}  {text[r]}")

    print()
    print(rule("inverse"))
    for r in range(ROWS):
        print(f"{r:2}  {video[r]}")

    print()
    print(rule("semigraphics"))
    for r in range(ROWS):
        print(f"{r:2}  {colour[r]}")

    used = sorted({c for row in colour for c in row} - {".", "-", " "})
    if used:
        print("    " + "  ".join(f"{c}={SG_FULL[c]}" for c in used))

    print()
    print(rule("quadrants"))
    for line in quadrants(mem):
        print("    " + line)


if __name__ == "__main__":
    main()
