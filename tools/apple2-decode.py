#!/usr/bin/env python3
"""
Decode an AppleWin save state into something readable.

Renders the 80x24 text screen back to ASCII out of the two text pages the
state file carries: even columns from auxiliary memory, odd from main. That is
the same split src/apple2enh/blit.s writes through, so what comes out here is
exactly what the blitter put in.

Three renderings, because one cannot carry all of it on a screen whose whole
vocabulary is "which of three character sets is this byte in":

  the text     inverse spaces as '#', MouseText as the nearest Unicode shape
  the inverse map   which cells are inverse video, since an inverse *letter*
                    reads the same as a normal one in the text above
  the MouseText map where the glyphs actually landed

Driven by tools/apple2-shot.sh.
"""
import re
import sys

SCR_COLS = 80
SCR_ROWS = 24

# Read out of AppleWin's Apple2e_Enhanced_Video.rom rather than remembered.
# Only the shapes src/apple2enh/platform.h names are load-bearing; the rest are
# here so an unexpected one is still recognisable.
MOUSETEXT = [
    "@",  # $40 closed apple
    "&",  # $41 open apple
    "➤",  # $42 pointer
    "⧗",  # $43 hourglass
    "✓",  # $44 check
    "✗",  # $45 inverse check
    "?",  # $46
    "?",  # $47
    "←",  # $48 left arrow
    "┈",  # $49 dotted rule
    "↓",  # $4A down arrow
    "↑",  # $4B up arrow
    "‾",  # $4C top rule
    "↵",  # $4D return
    "█",  # $4E solid block
    "▶",  # $4F
    "▷",  # $50
    "▼",  # $51
    "▲",  # $52
    "─",  # $53 centred rule
    "└",  # $54 bottom-left corner
    "→",  # $55 right arrow
    "░",  # $56 dither A
    "▒",  # $57 dither B
    "▙",  # $58 folder left
    "▟",  # $59 folder right
    "▕",  # $5A right vertical rule
    "◆",  # $5B diamond
    "≡",  # $5C two rules
    "⋮",  # $5D
    "▢",  # $5E
    "▏",  # $5F left vertical rule
]


def read_memory_block(lines, start):
    """Read the `AAAA: <hex>` lines that follow a memory label."""
    mem = bytearray(0x10000)
    for line in lines[start:]:
        m = re.match(r"\s*([0-9A-F]{4}):\s*([0-9A-F]+)\s*$", line)
        if not m:
            if line.strip() == "":
                continue
            break
        addr = int(m.group(1), 16)
        data = bytes.fromhex(m.group(2))
        mem[addr:addr + len(data)] = data
    return mem


def find_block(lines, label):
    for i, line in enumerate(lines):
        if line.strip() == label:
            return read_memory_block(lines, i + 1)
    sys.exit(f'no "{label}" block in the save state')


def classify(b):
    """Screen byte -> (character, inverse, mousetext).

    The enhanced //e alternate character set, which is what
    src/apple2enh/screen.c writes for:

        $00-$1F inverse uppercase    $20-$3F inverse symbols
        $40-$5F MouseText            $60-$7F inverse lowercase
        $80-$FF normal ASCII + $80
    """
    if b >= 0x80:
        return chr(b & 0x7F), False, False
    if b < 0x20:
        return chr(b + 0x40), True, False
    if b < 0x40:
        return chr(b), True, False
    if b < 0x60:
        return MOUSETEXT[b - 0x40], False, True
    return chr(b), True, False


def main(path):
    lines = open(path, encoding="latin1").read().splitlines()

    main_mem = find_block(lines, "Main Memory:")
    aux_mem = find_block(lines, "Auxiliary Memory Bank00:")

    print(f"     +{'-' * SCR_COLS}+")
    inv_rows = []
    mt_rows = []

    for row in range(SCR_ROWS):
        # Text page 1, the same arithmetic blit.s spells out in its row table.
        base = 0x400 + (row & 7) * 0x80 + (row >> 3) * 0x28

        text = []
        invmap = []
        mtmap = []
        for col in range(SCR_COLS):
            b = (aux_mem if (col & 1) == 0 else main_mem)[base + (col >> 1)]
            ch, inv, mt = classify(b)

            # An inverse space is the whole vocabulary of the selection bars,
            # the chrome bands and the month density bars. Left as a space it
            # is invisible, and those are the things most worth checking.
            if inv and ch == " ":
                ch = "#"
            text.append(ch)
            invmap.append("#" if inv else ".")
            mtmap.append(MOUSETEXT[b - 0x40] if mt else ".")

        print(f"  {row:2d} |{''.join(text)}|")
        inv_rows.append("".join(invmap))
        mt_rows.append("".join(mtmap))

    print(f"     +{'-' * SCR_COLS}+")

    print("\ninverse video:")
    for row, line in enumerate(inv_rows):
        if "#" in line:
            print(f"  {row:2d} |{line}|")

    print("\nMouseText:")
    any_mt = False
    for row, line in enumerate(mt_rows):
        if line.strip("."):
            print(f"  {row:2d} |{line}|")
            any_mt = True
    if not any_mt:
        print("  (none -- is ALTCHARSET on?)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: apple2-decode.py <state.yaml>")
    main(sys.argv[1])
