#!/usr/bin/env python3
"""Decode a SCREEN.BIN captured by the MS-DOS build's GM_SHOT hook.

The file is three header bytes (columns, rows, BIOS video mode) followed by
the text page verbatim: one char/attr pair per cell, CP437 glyphs. Prints
the text in a border. --attrs adds a second grid of the attribute bytes as
hex, which is how the selection bar, the unread emphasis and the MDA
underline get checked -- none of them are visible in the glyphs alone.

    tools/msdos-decode.py SCREEN.BIN
    tools/msdos-decode.py --attrs SCREEN.BIN
"""

import sys


def main() -> int:
    args = sys.argv[1:]
    show_attrs = "--attrs" in args
    args = [a for a in args if a != "--attrs"]
    if len(args) != 1:
        print(__doc__, file=sys.stderr)
        return 2

    data = open(args[0], "rb").read()
    if len(data) < 3:
        print("truncated capture", file=sys.stderr)
        return 1

    cols, rows, mode = data[0], data[1], data[2]
    page = data[3:3 + cols * rows * 2]
    if len(page) != cols * rows * 2:
        print(f"expected {cols}x{rows} page, got {len(page)} bytes",
              file=sys.stderr)
        return 1

    print(f"{cols}x{rows} mode {mode}")
    print("+" + "-" * cols + "+")
    for r in range(rows):
        row = page[r * cols * 2:(r + 1) * cols * 2]
        text = bytes(row[0::2]).decode("cp437")
        # Control-range CP437 glyphs decode to control characters; show the
        # glyphs instead so arrows and the unread diamond survive a terminal.
        glyphs = "\x00☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼"
        text = "".join(glyphs[ord(c)] if ord(c) < 32 else c for c in text)
        print("|" + text + "|")
    print("+" + "-" * cols + "+")

    if show_attrs:
        print()
        for r in range(rows):
            row = page[r * cols * 2:(r + 1) * cols * 2]
            print("".join(f"{b:02x}" for b in row[1::2]))

    return 0


if __name__ == "__main__":
    sys.exit(main())
