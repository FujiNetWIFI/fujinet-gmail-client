/*
 * Frame timing.
 *
 * The Atari reads RTCLOK, which the OS vertical blank increments whatever the
 * program is doing. There is no such counter on an Apple II, so this is one we
 * keep ourselves, bumped by every plat_vsync().
 *
 * That is enough because the blocking waits in input.c are polls around
 * plat_vsync() rather than firmware calls that never come back: the count goes
 * on rising while the inbox, the reader and the compose form sit waiting for a
 * key, which is the property tick_advance() actually needs.
 *
 * What it does not cover is a SmartPort transfer. Nobody calls plat_vsync()
 * while fujinet-lib is talking to the device, so the clock loses the length of
 * every fetch -- a second or two each, more for a cold index open. The
 * half-hour resync in tick.c is what bounds the drift.
 *
 * Ported from the calendar client's src/apple2enh/timer.c.
 */

#include <apple2.h>

#include "../gmail.h"
#include "platform.h"

static unsigned long ticks;
static unsigned char fps;

void plat_vsync(void)
{
    waitvsync();
    ticks++;
}

unsigned long plat_ticks(void)
{
    return ticks;
}

/*
 * Getting this backwards runs the clock 20% fast or slow, which across a
 * half-hour resync is six minutes. cc65 calibrates get_tv() against the
 * machine at startup, so it costs nothing to be right.
 */
unsigned char plat_fps(void)
{
    if (!fps)
        fps = (unsigned char) ((get_tv() == TV_PAL) ? 50 : 60);

    return fps;
}
