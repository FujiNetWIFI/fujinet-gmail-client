#!/usr/bin/env python3
"""
Drive applen headlessly and take a save state.

applen is AppleWin's ncurses frontend, built from the FujiNet fork -- the same
emulator core as sa2, so the SmartPort device relay works from it and a REAL=1
run reaches fujinet-pc exactly as an sa2 one would.

The save state is what gets decoded, not the terminal. applen's own rendering
would be simpler to capture, but MapCharacter() folds screen codes $00-$1F and
$40-$5F onto the same reversed '@'-'_' -- so inverse uppercase and MouseText
come out identical, and those are the two things worth checking. The state file
has the raw main and aux text pages instead.

Three things about driving it are easy to get wrong:

  - --state-filename is ignored unless the file already exists. setSnapshotFilename()
    guards on std::filesystem::exists(), so a fresh path silently falls back to
    the default and the save lands somewhere else. The placeholder below is
    what makes the option take effect.

  - --headless never creates the ncurses window, and ProcessKeys() reads
    through it, so F11 is never seen and no state is ever written. It has to
    run with the window, which also means it runs at real speed -- hence the
    wait.

  - initscr() is called with set_escdelay(0), so F11 has to arrive as one
    write or ncurses hands the emulator a bare ESC followed by four keystrokes.

Called by tools/apple2-shot.sh; not usually run by hand.
"""
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time

# The frame window is 1 + 24 + 1 rows by 1 + 80 + 1 columns, with an eight-row
# status window under it. Sized exactly so NFrame centres it at offset zero.
TERM_ROWS = 34
TERM_COLS = 82


def drain(fd, secs):
    """Absorb output for `secs`, returning early if the child closes."""
    end = time.time() + secs
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if not r:
            continue
        try:
            if not os.read(fd, 65536):
                return
        except OSError:
            return


def main(applen, disk, state, wait):
    # Has to exist before applen starts -- see the note above.
    open(state, "w").close()

    pid, fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "xterm"
        os.chdir(os.path.dirname(state))
        os.execv(applen, [applen, "--d1", disk, "--state-filename", state,
                          "--no-audio"])

    fcntl.ioctl(fd, termios.TIOCSWINSZ,
                struct.pack("HHHH", TERM_ROWS, TERM_COLS, 0, 0))

    drain(fd, wait)
    os.write(fd, b"\x1b[23~")           # F11, xterm's kf11
    drain(fd, 3.0)

    try:
        os.kill(pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    os.waitpid(pid, 0)

    if os.path.getsize(state) == 0:
        sys.exit("no save state -- applen never reached F11")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit("usage: apple2-run.py <applen> <disk.po> <state.yaml> <wait>")
    main(sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]))
