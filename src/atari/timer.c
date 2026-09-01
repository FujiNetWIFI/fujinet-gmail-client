/*
 * Frame timing.
 *
 * RTCLOK is the OS's own vertical-blank counter, and reading it rather than
 * counting our own frames is what keeps the clock honest across a screen this
 * program is not driving. It goes on rising whatever we are doing -- which on
 * this machine includes the whole of an SIO transfer, since the VBI runs on
 * the interrupt fujinet-lib does not mask.
 *
 * Ported from the calendar client's src/atari/timer.c, which needed it first.
 */

#include <atari.h>

#include "../gmail.h"
#include "platform.h"

#define RTCLOK  ((volatile unsigned char *) 0x12)
#define PALREG  ((volatile unsigned char *) 0xD014)

void plat_vsync(void)
{
    waitvsync();
}

unsigned long plat_ticks(void)
{
    unsigned long a, b;

    /* The vertical blank can carry between the byte reads, so take it twice
       and only trust a pair that agrees. */
    do {
        a = ((unsigned long) RTCLOK[0] << 16) |
            ((unsigned long) RTCLOK[1] << 8) | RTCLOK[2];
        b = ((unsigned long) RTCLOK[0] << 16) |
            ((unsigned long) RTCLOK[1] << 8) | RTCLOK[2];
    } while (a != b);

    return a;
}

/*
 * PAL reads $01 from GTIA's PAL register and NTSC reads $0F. Getting this
 * backwards runs the clock 20% fast or slow, which across a half-hour resync
 * is six minutes -- visible enough to be worth the two bytes of test.
 */
unsigned char plat_fps(void)
{
    return (unsigned char) ((*PALREG & 0x0E) ? 60 : 50);
}
