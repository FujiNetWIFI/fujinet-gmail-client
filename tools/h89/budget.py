#!/usr/bin/env python3
"""The H89 CP/M build's memory budget, read from the z88dk map.

z88dk's CP/M startup sets the stack pointer to the BDOS entry at (0006) and
lets the program run over the CCP, which the warm boot on exit reloads. So
everything from 0100H to the end of BSS, plus the stack, has to sit under
the BDOS of the smallest system the program runs on:

  62K CP/M (MOVCPM17 62 -- what FNDISK needs)   BDOS entry D806H
  64K CP/M (the emulator's boot disk)           BDOS entry E006H

Past that it does not fail politely: the startup zeroes BSS, which then
includes the BDOS, and the program hangs on its first system call. This
fails the build instead.

The .COM is the other limit. z88dk writes BSS into it as zeros, which the
startup clears again anyway (CRT_INITIALIZE_BSS) -- and the CCP will only
load a file that ends below itself, D000H on the 62K system, so the zeros
alone made it a BAD LOAD there. --trim cuts each .COM named off where BSS
begins, which also takes a third off the time an H17 spends loading it.

  budget.py MAP [--stack BYTES] [--trim COM...]
"""

import re
import sys

BDOS_62K = 0xD806
BDOS_64K = 0xE006
CCP_62K = 0xD000


def main():
    args = sys.argv[1:]
    stack = 1024
    trim = []
    if "--trim" in args:
        i = args.index("--trim")
        trim = args[i + 1:]
        del args[i:]
    if "--stack" in args:
        i = args.index("--stack")
        stack = int(args[i + 1], 0)
        del args[i:i + 2]
    if len(args) != 1:
        sys.exit(__doc__)

    syms = {}
    with open(args[0]) as f:
        for line in f:
            m = re.match(r"^(\w+)\s*=\s*\$([0-9A-Fa-f]+)\s*;", line)
            if m:
                syms[m.group(1)] = int(m.group(2), 16)

    end = max(syms.get("__tail", 0), syms.get("__BSS_END_tail", 0))
    if not end:
        sys.exit("budget.py: no __tail in %s" % args[0])

    code = syms.get("__code_compiler_size", 0)
    bss = syms.get("__bss_compiler_size", 0)
    top = end + stack
    print("h89 budget: end of program %04XH (%d bytes from 0100H), "
          "+%d stack = %04XH" % (end, end - 0x100, stack, top))
    print("h89 budget: compiled code %d, compiled BSS %d" % (code, bss))
    print("h89 budget: 62K CP/M ceiling %04XH: %+d spare; "
          "64K %04XH: %+d spare" % (BDOS_62K, BDOS_62K - top,
                                    BDOS_64K, BDOS_64K - top))

    bss = syms.get("__BSS_head", 0)
    if trim and bss:
        if syms.get("__BSS_END_tail", end) != end:
            sys.exit("h89 budget: BSS is not last; cannot trim the .COM")
        for com in trim:
            with open(com, "r+b") as f:
                f.truncate(bss - 0x100)
        print("h89 budget: .COM cut to %d bytes, ending %04XH: %+d under the "
              "62K CCP" % (bss - 0x100, bss, CCP_62K - bss))
        if bss > CCP_62K:
            sys.exit("h89 budget: the .COM is past the 62K CCP -- BAD LOAD")

    if top > BDOS_64K:
        sys.exit("h89 budget: OVER even a 64K CP/M -- it will hang at load")
    if top > BDOS_62K:
        sys.exit("h89 budget: OVER the 62K CP/M that FNDISK needs")


if __name__ == "__main__":
    main()
