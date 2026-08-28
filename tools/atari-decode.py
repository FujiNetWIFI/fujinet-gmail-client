#!/usr/bin/env python3
"""
Decode an atari800 full-RAM dump into something readable.

Renders the 40x24 text screen back to ASCII, shows which display list rows
carry an interrupt bit, and draws the player/missile logo as it will actually
appear on screen. Driven by tools/atari-shot.sh.
"""
import sys

PM_BASE = 0xB800        # 1K-aligned P/M buffer, see src/atari/pmg.c
PM_P0 = PM_BASE + 0x200
PM_STRIDE = 0x80


def main(path):
    mem = open(path, 'rb').read()
    if len(mem) != 65536:
        sys.exit(f"expected a 64K dump, got {len(mem)} bytes")

    def word(a):
        return mem[a] | (mem[a + 1] << 8)

    savmsc = word(0x58)
    dl = word(0x230)

    print(f"SAVMSC = ${savmsc:04X}   SDLST = ${dl:04X}   "
          f"MEMTOP = ${word(0x2E5):04X}   APPMHI = ${word(0x0E):04X}")

    # ---- text screen -------------------------------------------------
    def unscr(c):
        """Atari screen code back to ASCII, inverse bit stripped."""
        v = c & 0x7F
        if v < 0x40:
            return chr(v + 0x20)
        if v < 0x60:
            return chr(v - 0x40)
        return chr(v)

    print("\n     +" + "-" * 40 + "+")
    for row in range(24):
        cells = mem[savmsc + row * 40:savmsc + row * 40 + 40]
        text = "".join(unscr(c) for c in cells)
        inv = "  <-INVERSE" if any(c & 0x80 for c in cells) else ""
        print(f"  {row:2d} |{text}|{inv}")
    print("     +" + "-" * 40 + "+")

    # ---- display list ------------------------------------------------
    print(f"\ndisplay list @ ${dl:04X}:")
    raw = mem[dl:dl + 32]
    print("  " + " ".join(f"{b:02X}" for b in raw))

    dli_rows, row, i = [], 0, 0
    while i < 32:
        b = raw[i]
        if (b & 0x0F) == 0:             # blank scanlines
            i += 1
            continue
        if (b & 0x0F) == 1:             # jump / JVB ends the list
            break
        if b & 0x80:
            dli_rows.append(row)
        row += 1
        i += 4 if (b & 0x40) else 1     # LMS carries a 2-byte address
    print(f"  text rows carrying a DLI bit: {dli_rows}   (expect [2, 22])")

    # ---- player/missile ----------------------------------------------
    print(f"\nP/M buffer @ ${PM_BASE:04X} (double-line, 128 bytes/player)")
    names = ["P0 red diag", "P1 yellow diag", "P2 blue pillar", "P3 green pillar"]
    spans = []
    for p in range(4):
        data = mem[PM_P0 + p * PM_STRIDE:PM_P0 + p * PM_STRIDE + PM_STRIDE]
        nz = [(i, b) for i, b in enumerate(data) if b]
        spans.append(nz)
        if not nz:
            print(f"  {names[p]:16s}: empty")
        else:
            print(f"  {names[p]:16s}: bytes ${nz[0][0]:02X}..${nz[-1][0]:02X}  "
                  + " ".join(f"{b:02X}" for _, b in nz))

    if not any(spans):
        return

    # HPOS reads back unreliably from a dump; these are the values pmg.c sets
    # for the logo, offset from its left edge.
    hpos = [6, 22, 0, 38]
    colour = "RYBG"
    top = min(nz[0][0] for nz in spans if nz)
    bot = max(nz[-1][0] for nz in spans if nz)

    print("\nlogo as rendered (each player bit = 2 colour clocks at double width):")
    for r in range(top, bot + 1):
        line = [' '] * 64
        for p in range(4):
            b = mem[PM_P0 + p * PM_STRIDE + r]
            for bit in range(8):
                if b & (0x80 >> bit):
                    x = hpos[p] + bit * 2
                    line[x] = line[x + 1] = colour[p]
        print("    " + "".join(line).rstrip())


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__))
