#!/usr/bin/env python3
"""Render what a program sends an H19, from a byte stream on stdin.

Enough of the Heath-mode terminal for this program's output: cursor
addressing, reverse video, graphics mode, erase to end of line / line /
screen, insert and delete line, the 25th line, discard at end of line. Any
escape it does not know is reported, which is half the point.

At each marker tools/h89/hostcon.c writes (FF "SHOT" FF) -- and at the end --
it prints the 25 lines as text, with graphics characters as their Unicode
look-alikes, and under each a line marking the reverse-video cells with #.

  h19model.py [--attrs] < stream
"""

import sys

GFX = {0x5E: "●", 0x5F: "◥", 0x60: "│", 0x61: "─", 0x62: "┼", 0x63: "┐",
       0x64: "┘", 0x65: "└", 0x66: "┌", 0x67: "±", 0x68: "→", 0x69: "▒",
       0x6A: "÷", 0x6B: "↓", 0x6C: "▗", 0x6D: "▖", 0x6E: "▘", 0x6F: "▝",
       0x70: "▀", 0x71: "▐", 0x72: "◤", 0x73: "┬", 0x74: "┤", 0x75: "┴",
       0x76: "├", 0x77: "╳", 0x78: "╱", 0x79: "╲", 0x7A: "▔", 0x7B: "▁",
       0x7C: "▏", 0x7D: "▕", 0x7E: "¶"}


class H19:
    def __init__(self):
        self.cells = [[(" ", False)] * 80 for _ in range(25)]
        self.x = self.y = 0
        self.rev = self.gfx = False
        self.line25 = False
        self.wrap = False
        self.cursor_on = True
        self.unknown = []

    def put(self, ch):
        if self.x >= 80:
            if not self.wrap:
                return
            self.x = 0
            if self.y < 23:
                self.y += 1
        g = GFX.get(ch) if self.gfx else None
        self.cells[self.y][self.x] = (g or chr(ch), self.rev)
        if self.x < 79 or self.wrap:
            self.x += 1

    def run(self, data, shot):
        i = 0
        n = len(data)
        while i < n:
            c = data[i]
            i += 1
            if c == 0xFF and data[i:i + 5] == b"SHOT\xff":
                i += 5
                shot(self, i)
                continue
            if c == 0x1B:
                e = data[i]
                i += 1
                if e == ord("Y"):
                    r, col = data[i] - 32, data[i + 1] - 32
                    i += 2
                    if r == 24 and not self.line25:
                        r = self.y
                    if 0 <= r < 25:
                        self.y = r
                    self.x = min(col, 79)
                elif e == ord("p"):
                    self.rev = True
                elif e == ord("q"):
                    self.rev = False
                elif e == ord("F"):
                    self.gfx = True
                elif e == ord("G"):
                    self.gfx = False
                elif e == ord("K"):
                    for x in range(self.x, 80):
                        self.cells[self.y][x] = (" ", False)
                elif e == ord("l"):
                    self.cells[self.y] = [(" ", False)] * 80
                elif e == ord("E"):
                    rows = [24] if self.y == 24 else range(24)
                    for r in rows:
                        self.cells[r] = [(" ", False)] * 80
                    if self.y != 24:
                        self.x = self.y = 0
                elif e == ord("L"):
                    if self.y < 24:
                        self.cells.insert(self.y, [(" ", False)] * 80)
                        del self.cells[24]
                    self.x = 0
                elif e == ord("M"):
                    if self.y < 24:
                        del self.cells[self.y]
                        self.cells.insert(23, [(" ", False)] * 80)
                    self.x = 0
                elif e in (ord("x"), ord("y")):
                    m = data[i]
                    i += 1
                    if m == ord("1"):
                        self.line25 = e == ord("x")
                    elif m == ord("5"):
                        self.cursor_on = e == ord("y")
                elif e == ord("w"):
                    self.wrap = False
                elif e == ord("v"):
                    self.wrap = True
                elif e in (ord("t"), ord("u"), ord("z"), ord("="), ord(">")):
                    pass
                else:
                    self.unknown.append(e)
                continue
            if c == 0x07:
                continue
            if c == 0x0D:
                self.x = 0
                continue
            if c == 0x0A:
                self.y = min(self.y + 1, 23)
                continue
            if c < 0x20:
                self.unknown.append(c)
                continue
            self.put(c)


def main():
    attrs = "--attrs" in sys.argv
    data = sys.stdin.buffer.read()
    count = [0]

    def shot(t, at):
        count[0] += 1
        print("==== shot %d ====" % count[0])
        for r in range(25):
            print("".join(ch for ch, _ in t.cells[r]).rstrip())
            if attrs and any(rv for _, rv in t.cells[r]):
                print("".join("#" if rv else "." for _, rv in t.cells[r]).rstrip("."))
        print("==== %d bytes sent so far ====" % at)

    t = H19()
    t.run(data, shot)
    if t.unknown:
        print("unknown escapes/controls:", sorted(set(t.unknown)), file=sys.stderr)


if __name__ == "__main__":
    main()
