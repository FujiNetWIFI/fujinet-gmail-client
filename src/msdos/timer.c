/*
 * Frame timing off the BIOS tick.
 *
 * The BDA dword at 0040:006C counts timer-tick interrupts, 18.2 per second,
 * from power-on -- the PC's RTCLOK. It advances in the background whatever
 * this program is doing, which is the property tick.c actually needs, and it
 * keeps moving through a FUJINET.SYS transfer as well: the driver does not
 * mask the timer interrupt.
 *
 * plat_fps() says 18, not 60. tick.c divides elapsed ticks by this to step
 * seconds, so the honest answer matters -- calling it 60 would run the clock
 * at a third speed. The 0.2 left over is ~1% drift, absorbed by the half-hour
 * resync, and the wrap (the BIOS zeroes the count at midnight, read back as
 * now < last) is the same case tick_advance() already treats as costing at
 * most one pass.
 *
 * Ported from the calendar client's src/msdos/timer.c.
 */

#include <dos.h>

#include "../gmail.h"
#include "platform.h"

/*
 * INT 08 can fire between the two halves of a 16-bit read of a 32-bit value,
 * so read until two reads agree -- the classic torn-read guard, and two
 * instructions cheaper than cli/sti.
 */
static unsigned long peek_ticks(void)
{
    unsigned long far *p = MK_FP(0x0040, 0x006C);
    unsigned long a, b;

    do {
        a = *p;
        b = *p;
    } while (a != b);

    return a;
}

/* One frame here is one BIOS tick: a key poll turns 18 times a second, which
   bounds the poll rate without a busy spin on the keyboard. */
void plat_vsync(void)
{
    unsigned long t = peek_ticks();

    while (peek_ticks() == t)
        ;
}

unsigned long plat_ticks(void)
{
    return peek_ticks();
}

unsigned char plat_fps(void)
{
    return 18;
}
