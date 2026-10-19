#!/usr/bin/env python3
"""The H89 HDOS build's overlays: checked, budgeted and packed.

  hdosovl.py BASE --objs DIR --win ADDR --size BYTES --top ADDR [--row BYTES]

BASE is the link's output without its suffix (r2r/h89-hdos/gmail): BASE.map,
BASE.abs, and the BASE_OVL_<name>.bin z80asm wrote for each overlay that
src/h89/mmap.inc declares.

check  z80asm links a call from one overlay into another without a murmur --
       both are just addresses in the window -- and at run time it lands in
       whatever is loaded. So, from each object's expressions (z88dk-z80nm),
       nothing may refer into an overlay but that overlay itself, except a
       banked call's defq (a 4-byte expression), which goes through
       banked_call. A failure names both ends.

budget Each overlay has to fit the window, and the end of BSS has to be under
       --top: S.SYSM - S.OMAX with FN: loaded, the most HDOS leaves once it
       is. src/h89/tpa.asm makes the same check on the machine.

pack   GMAIL.ABS as HDOS loads it -- the 8-byte header, the image up to BSS
       -- padded to a sector, then each overlay, padded to one: the layout
       src/h89/ovl.asm's _ovl_tab was linked for, which is checked against
       what is packed.

On any failure BASE.abs is removed, so the next make links again.
"""

import argparse
import os
import re
import subprocess
import sys

OVERLAYS = ["FETCH", "BROWSE", "COMPOSE"]      # src/h89/mmap.inc's order
SECTOR = 256


def fail(base, msg):
    try:
        os.remove(base + ".abs")
    except OSError:
        pass
    sys.exit("h89 hdos: " + msg)


def read_map(path):
    """name -> address, and (module, name) -> address for the locals."""
    glob, loc = {}, {}
    for line in open(path):
        m = re.match(r"^(\w+)\s*=\s*\$([0-9A-Fa-f]+)\s*;\s*(\w+),\s*(\w+),[^,]*,\s*(\w*)", line)
        if not m:
            continue
        name, val, scope, module = m.group(1), int(m.group(2), 16), m.group(4), m.group(5)
        if scope == "public" or module == "":
            glob[name] = val
        loc[(module, name)] = val
    return glob, loc


def overlay_of_section(sec):
    m = re.match(r"^(?:code|rodata)_ovl_(\w+)$", sec)
    return m.group(1).upper() if m else None


def check(base, objdir, glob, loc):
    """Every reference into an overlay comes from inside it or is a defq."""
    num = {n: i + 1 for i, n in enumerate(OVERLAYS)}
    objs = []
    for root, _, files in os.walk(objdir):
        objs += [os.path.join(root, f) for f in files if f.endswith(".o")]
    bad = []
    for obj in sorted(objs):
        out = subprocess.run(["z88dk-z80nm", "-a", obj], capture_output=True,
                             text=True, check=True).stdout
        module = re.search(r"^\s*Name:\s*(\S+)", out, re.M).group(1)
        part = None
        for line in out.splitlines():
            if line.strip() in ("Symbols:", "Externs:", "Expressions:"):
                part = line.strip()
                continue
            if part != "Expressions:":
                continue
            m = re.match(r"^\s*E (\w+)\s+\$\w+\s+\$\w+\s+\d+:\s*(.*?)\s+\(section (\S+)\)", line)
            if not m:
                continue
            kind, expr, sec = m.groups()
            long_ = kind in ("L", "S32", "U32")     # z80nm's names vary by version
            here = overlay_of_section(sec)
            for sym in re.findall(r"[A-Za-z_]\w*", expr):
                if re.match(r"^__\w+_(head|tail|size)$", sym):
                    continue            # the linker's section bounds
                val = loc.get((module, sym), glob.get(sym))
                if val is None or val < 0x10000:
                    continue
                there = OVERLAYS[(val >> 16) - 1]
                if there == here or long_:
                    continue
                bad.append("%s (%s, %s) refers to %s in overlay %s"
                           % (os.path.relpath(obj, objdir), sec,
                              here or "resident", sym, there))
    if bad:
        fail(base, "a reference into an overlay from outside it -- "
             "a missing GM_OVL?\n  " + "\n  ".join(bad))


def main():
    ap = argparse.ArgumentParser(usage=__doc__)
    ap.add_argument("base")
    ap.add_argument("--objs", required=True)
    ap.add_argument("--win", type=lambda v: int(v, 0), required=True)
    ap.add_argument("--size", type=lambda v: int(v, 0), required=True)
    ap.add_argument("--top", type=lambda v: int(v, 0), required=True)
    ap.add_argument("--row", type=int, default=77)
    a = ap.parse_args()
    base = a.base

    glob, loc = read_map(base + ".map")
    check(base, a.objs, glob, loc)

    # --- budget
    sizes = {}
    for n in OVERLAYS:
        sizes[n] = (glob["__OVL_%s_END_head" % n] - glob["__OVL_%s_head" % n]) & 0xFFFF
    abs_ = open(base + ".abs", "rb").read()
    if abs_[:2] != b"\xff\x00":
        fail(base, "%s.abs has no ABS header" % base)
    load = abs_[2] | abs_[3] << 8
    length = abs_[4] | abs_[5] << 8
    bss, end = glob["__BSS_head"], glob["__BSS_END_tail"]
    if load != a.win + a.size:
        fail(base, "the program is at %04XH, not after the window (%04XH)" % (load, a.win + a.size))
    print("h89 hdos: stack 2280H, window %04XH-%04XH (%d): %s"
          % (a.win, a.win + a.size - 1, a.size,
             ", ".join("%s %d" % (n, sizes[n]) for n in OVERLAYS)))
    print("h89 hdos: program %04XH-%04XH, BSS to %04XH: %+d under %04XH (%d bytes a body row)"
          % (load, bss - 1, end, a.top - end, a.top, a.row))
    big = [n for n in OVERLAYS if sizes[n] > a.size]
    if big:
        fail(base, "%s past the window: raise H89_OVL_SIZE" % ", ".join(big))
    if end > a.top:
        fail(base, "BSS ends past %04XH: lower H89_HDOS_BODY_ROWS by %d"
             % (a.top, (end - a.top + a.row - 1) // a.row))

    # --- pack
    if length != bss - load or len(abs_) != 8 + length:
        fail(base, "%s.abs is %d bytes for an image of %d" % (base, len(abs_), bss - load))
    out = bytearray(abs_)
    out += bytes(-len(out) % SECTOR)
    want = []
    for n in OVERLAYS:
        path = "%s_OVL_%s.bin" % (base, n)
        data = open(path, "rb").read() if os.path.exists(path) else b""
        if len(data) != sizes[n]:
            fail(base, "%s is %d bytes, the map says %d" % (path, len(data), sizes[n]))
        want += [len(out) // SECTOR, (len(data) + SECTOR - 1) // SECTOR]
        out += data + bytes(-len(data) % SECTOR)
    tab = glob["_ovl_tab"] - load + 8
    have = []
    for i in range(len(OVERLAYS)):
        e = abs_[tab + 3 * i:tab + 3 * i + 3]
        have += [e[0] | e[1] << 8, e[2]]
    if have != want:
        fail(base, "_ovl_tab says %s, the file is %s" % (have, want))
    open(base + ".abs", "wb").write(out)
    for n in OVERLAYS + ["BSS"]:
        try:
            os.remove("%s_%s.bin" % (base, n) if n == "BSS" else "%s_OVL_%s.bin" % (base, n))
        except OSError:
            pass
    print("h89 hdos: %s.abs %d sectors, HDOS loads %d" % (base, len(out) // SECTOR, want[0]))


if __name__ == "__main__":
    main()
