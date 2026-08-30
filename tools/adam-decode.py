#!/usr/bin/env python3
"""
Render the Adam's GRAPHICS II screen out of an ADAMEm snapshot.

The other three backends can be checked from a pane of text, because on those
machines a screen *is* text: a byte per cell, and the colour either does not
exist or is a field inside that byte. This one is a 256x192 bitmap with an
independent foreground and background per 8x1 strip, and the parts of the
client that only exist here -- eleven distinct Google colours, a 32-step
density bar, a four-sprite mark -- are not checkable from glyphs at all.

So the primary output is a picture. Two text panes come with it:

  attrs       one letter per cell for the background ink, which is what shows
              the header band, the selection bar, the chip gutter and the
              MONTH bars without needing to recognise a glyph
  sprites     the sprite attribute table, decoded, plus a per-scanline count

That last pane is the one worth reading carefully. A TMS9918A drops the fifth
sprite on any scanline, and the header mark deliberately sits on four, so a
count of five anywhere means part of the mark has silently gone missing.

Usage: adam-decode.py <snapshot> [-o out.png]
"""

import sys

# _SaveSnapshotFile() in adamem_sdl/Coleco.c writes fixed-size blocks:
# 16 magic + 2 version + 1 EmuMode + 1 RAMPages + 256 cart + 4*256 disk
# + 4*256 tape + 31 regs + 4 ICount + 36 misc = 2395, then the VDP block.
VDP_REG_OFF = 2395
VRAM_OFF    = 2434
VRAM_SIZE   = 16384

# z88dk's mode 2 map (libsrc/classic/video/tms9918/__vdp_mode2.asm).
PATTERN   = 0x0000
NAME      = 0x1800
SPR_ATTR  = 0x1B00
COLOUR    = 0x2000
SPR_GEN   = 0x3800

COLS, ROWS = 32, 24

# The TMS9918A's fifteen inks, plus transparent.
PALETTE = [
    (0, 0, 0), (0, 0, 0), (33, 200, 66), (94, 220, 120),
    (84, 85, 237), (125, 118, 252), (212, 82, 77), (66, 235, 245),
    (252, 85, 84), (255, 121, 120), (212, 193, 84), (230, 206, 128),
    (33, 176, 59), (201, 91, 186), (204, 204, 204), (255, 255, 255),
]

# One letter per ink, for the attribute pane.
INKNAME = ".kGgBbRCrpYyDMwW"


def load(path):
    data = open(path, "rb").read()
    if data[:15] != b"ADAMEm snapshot":
        sys.exit("not an ADAMEm snapshot: %s" % path)
    if len(data) < VRAM_OFF + VRAM_SIZE:
        sys.exit("snapshot is short: %d bytes" % len(data))
    return data[VDP_REG_OFF:VDP_REG_OFF + 8], data[VRAM_OFF:VRAM_OFF + VRAM_SIZE]


def check_regs(reg):
    """The decode hardcodes z88dk's table addresses, so say so if they moved."""
    want = {2: NAME >> 10, 5: SPR_ATTR >> 7, 6: SPR_GEN >> 11}
    for r, v in want.items():
        if reg[r] != v:
            print("warning: VDP reg%d is 0x%02X, expected 0x%02X "
                  "(tables have moved; the decode below is wrong)"
                  % (r, reg[r], v), file=sys.stderr)
    if not (reg[0] & 0x02) or (reg[1] & 0x18):
        print("warning: not in GRAPHICS II (reg0=0x%02X reg1=0x%02X)"
              % (reg[0], reg[1]), file=sys.stderr)


def background(vram, backdrop):
    """192 rows of 256 ink indices, sprites not yet composited."""
    px = [[backdrop] * 256 for _ in range(192)]

    for row in range(ROWS):
        bank = (row // 8) * 2048
        for col in range(COLS):
            name = vram[NAME + row * COLS + col]
            base = bank + name * 8
            for line in range(8):
                bits = vram[PATTERN + base + line]
                attr = vram[COLOUR + base + line]
                fg, bg = attr >> 4, attr & 15
                if fg == 0:
                    fg = backdrop
                if bg == 0:
                    bg = backdrop
                out = px[row * 8 + line]
                for bit in range(8):
                    out[col * 8 + bit] = fg if (bits & (0x80 >> bit)) else bg
    return px


def sprites(vram, reg):
    """Decode the attribute table. Returns (list of sprites, per-line counts)."""
    big = bool(reg[1] & 0x02)
    mag = bool(reg[1] & 0x01)
    size = (16 if big else 8) * (2 if mag else 1)

    out, lines = [], [0] * 192
    for i in range(32):
        a = SPR_ATTR + i * 4
        y, x, pat, col = vram[a], vram[a + 1], vram[a + 2], vram[a + 3]
        if y == 208:
            break                       # end of the sprite list
        top = (y + 1) & 0xFF
        if top > 192:
            top -= 256                  # partially visible off the top
        if col & 0x80:
            x -= 32                     # early clock
        s = dict(id=i, x=x, y=top, pat=pat, ink=col & 15, size=size)
        out.append(s)
        for line in range(top, top + size):
            if 0 <= line < 192:
                lines[line] += 1
    return out, lines, big, mag


def composite(px, vram, sprs, big, mag):
    step = 2 if mag else 1
    for s in reversed(sprs):            # lower id wins, so paint high first
        if s["ink"] == 0:
            continue
        base = SPR_GEN + (s["pat"] & (0xFC if big else 0xFF)) * 8
        n = 16 if big else 8
        for dy in range(n):
            for dx in range(n):
                # 16x16 patterns are four 8x8 quadrants: TL, BL, TR, BR.
                if big:
                    q = (2 if dx >= 8 else 0) + (1 if dy >= 8 else 0)
                    byte = vram[base + q * 8 + (dy & 7)]
                    bit = 0x80 >> (dx & 7)
                else:
                    byte = vram[base + dy]
                    bit = 0x80 >> dx
                if not (byte & bit):
                    continue
                for sy in range(step):
                    for sx in range(step):
                        py, pxx = s["y"] + dy * step + sy, s["x"] + dx * step + sx
                        if 0 <= py < 192 and 0 <= pxx < 256:
                            px[py][pxx] = s["ink"]


def attr_pane(vram):
    print("attrs -- background ink per cell. The eleven Google colours are")
    print("         b Lavender  g Sage   M Grape  p Flamingo  y Banana  "
          "r Tangerine")
    print("         C Peacock   w Graphite  B Blueberry  D Basil  R Tomato")
    print("         and the chrome uses k black, W white, w gray, "
          "B dark blue, Y dark yellow, G med green.")
    print("     " + "".join(str(c % 10) for c in range(COLS)))
    for row in range(ROWS):
        bank = (row // 8) * 2048
        line = ""
        for col in range(COLS):
            name = vram[NAME + row * COLS + col]
            # Row 4 of the cell: past any ascender, inside any bar.
            attr = vram[COLOUR + bank + name * 8 + 4]
            line += INKNAME[attr & 15]
        print(" %2d  %s" % (row, line))


def sprite_pane(sprs, lines, big, mag):
    print()
    print("sprites -- %dx%d%s" % (16 if big else 8, 16 if big else 8,
                                  ", magnified" if mag else ""))
    if not sprs:
        print("  (none)")
    for s in sprs:
        print("  %2d  x=%-4d y=%-4d pattern=%-4d ink=%s"
              % (s["id"], s["x"], s["y"], s["pat"], INKNAME[s["ink"]]))

    over = [i for i, n in enumerate(lines) if n > 4]
    if over:
        print("  ** %d scanlines carry more than four sprites (%d..%d): "
              "the fifth is dropped by the hardware"
              % (len(over), over[0], over[-1]))
    else:
        busiest = max(lines) if lines else 0
        print("  per-scanline maximum: %d (the hardware shows four)" % busiest)


def main():
    args = [a for a in sys.argv[1:]]
    out = "adam.png"
    if "-o" in args:
        i = args.index("-o")
        out = args[i + 1]
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__.strip().splitlines()[-1])

    reg, vram = load(args[0])
    check_regs(reg)

    backdrop = reg[7] & 15
    px = background(vram, backdrop)
    sprs, lines, big, mag = sprites(vram, reg)
    composite(px, vram, sprs, big, mag)

    attr_pane(vram)
    sprite_pane(sprs, lines, big, mag)

    from PIL import Image
    img = Image.new("RGB", (256, 192))
    img.putdata([PALETTE[i] for row in px for i in row])
    img = img.resize((512, 384), Image.NEAREST)
    img.save(out)
    print()
    print("wrote %s" % out)


if __name__ == "__main__":
    main()
