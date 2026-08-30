#!/usr/bin/env python3
"""
Drive xroar's GDB target far enough to photograph the screen.

Run the machine, wait until the PC lands inside the address range given -- in
practice plat_vsync(), the frame wait the keyboard poll comes to rest in once
its scripted keys are spent -- and dump the 512 bytes of the 32x16 text page at
$0400 as hex on stdout. That is the entire visual state of this program -- text,
inverse video, semigraphics colour and quadrant masks alike -- so a capture
needs nothing else.

Four things about this target are worth knowing, and each cost an afternoon:

  - It halts the moment a connection is accepted (gdb.c's `gdb_machine_signal`
    on accept), so packets are accepted straight away with no interrupt first.
    But the listen backlog is one deep and accept() runs off the emulator
    thread, so a connect can succeed at the TCP level before anything is
    reading it: the first '?' has to be retried.

  - While the machine is running it answers every packet with '-' rather than
    queueing it. The only way to get its attention again is a raw 0x03.

  - 'Z0' breakpoints are accepted, answer OK, and never fire -- at least for
    RAM addresses on a coco2bus. Hence the poll below: interrupt, read the PC,
    and continue if it is not where we want it. It uses only the two primitives
    that do work, and lands on exactly the same instant a breakpoint would.

  - xroar's own -timeout counts *emulated* seconds. Paired with -no-ratelimit
    it fires before the machine finishes booting. tools/coco-shot.sh sets it
    absurdly high and relies on the socket timeout here instead.

Usage: coco-rsp.py <port> <lo-hex> <hi-hex> [timeout-seconds]
"""

import socket
import sys
import time

# 6809 'g' reply: CC A B DP X Y U S PC, 14 bytes, so the PC is the last 4 nybbles
# of the 28 significant ones.
PC_SLICE = slice(24, 28)


class Rsp:
    def __init__(self, port, timeout):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=10)
        self.s.settimeout(timeout)
        self.buf = b""

    def send(self, data):
        cs = sum(data.encode()) & 0xFF
        self.s.sendall(f"${data}#{cs:02x}".encode())

    def reply(self):
        """Next packet body, skipping the '+'/'-' acks around it."""
        while b"$" not in self.buf or b"#" not in self.buf.split(b"$", 1)[1]:
            b = self.s.recv(4096)
            if not b:
                raise EOFError("xroar closed the connection")
            self.buf += b

        _, rest = self.buf.split(b"$", 1)
        body, rest = rest.split(b"#", 1)
        while len(rest) < 2:
            self.buf += self.s.recv(4096)
            _, r2 = self.buf.split(b"$", 1)
            _, rest = r2.split(b"#", 1)

        self.buf = rest[2:]
        self.s.sendall(b"+")
        return body.decode()

    def cmd(self, data):
        self.send(data)
        return self.reply()

    def interrupt(self):
        """Stop the machine and throw away whatever it says about it.

        Draining rather than parsing the stop reply is what keeps this
        resynchronising. A '?' that timed out during the handshake is still
        sitting in xroar's socket and gets answered later, and an interrupt
        sent to an already-stopped machine is answered not at all -- so the
        number of replies outstanding is not something the client can track.
        Every poll therefore starts from an empty buffer instead.
        """
        self.s.sendall(b"\x03")

        old = self.s.gettimeout()
        self.s.settimeout(0.3)
        try:
            while self.s.recv(4096):
                pass
        except (socket.timeout, TimeoutError):
            pass
        finally:
            self.s.settimeout(old)

        self.buf = b""


def main():
    port = int(sys.argv[1])
    lo = int(sys.argv[2], 16)
    hi = int(sys.argv[3], 16)
    timeout = float(sys.argv[4]) if len(sys.argv) > 4 else 120.0

    r = Rsp(port, timeout)

    r.s.settimeout(2.0)
    for _ in range(20):
        try:
            r.cmd("?")
            break
        except (socket.timeout, TimeoutError):
            continue
    else:
        sys.exit("xroar never answered the initial '?'")
    r.s.settimeout(timeout)

    deadline = time.time() + timeout
    pc = -1
    while time.time() < deadline:
        r.send("c")
        time.sleep(0.5)
        r.interrupt()

        # A desynchronised exchange answers short or empty rather than with
        # fourteen register bytes. Continuing resynchronises: the next 'c' is
        # accepted whether the machine is stopped or already running.
        g = r.cmd("g")
        if len(g) < 28:
            continue

        pc = int(g[PC_SLICE], 16)
        if lo <= pc < hi:
            break
    else:
        where = f"${pc:04X}" if pc >= 0 else "nowhere legible"
        sys.exit(f"never reached ${lo:04X}..${hi:04X}; PC sits at {where}")

    screen = r.cmd("m400,200")
    if len(screen) != 1024:
        sys.exit(f"short screen read: {len(screen)} hex digits")

    print(screen)


if __name__ == "__main__":
    main()
