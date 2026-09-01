/*
 * Frame timing.
 *
 * One frame is a HALT rather than a counter read: the VDP raises an NMI once
 * per frame and z88dk's coleco crt installs a handler for it unconditionally,
 * so HALT wakes once per frame with no interrupt of our own to install and no
 * multi-byte counter maintained behind our back to read twice. It also keeps
 * the key poll off the AdamNet bus for sixteen milliseconds at a time, which a
 * bare spin on eos_end_read_keyboard() would not.
 *
 * That it wakes at all is worth having checked rather than reasoned about, and
 * it has been: counting the wakes into a global and reading it out of two
 * ADAMEm snapshots twenty seconds apart gave 1204 frames, which is 60.2 Hz --
 * the NTSC rate, so every HALT is being ended by the VDP and none of them by
 * anything else. The calendar client reaches the same place through
 * add_raster_int(), and needs to only because its HAL exposes a tick count.
 *
 * Counting here rather than in an interrupt is what makes plat_ticks() a plain
 * read with no torn-read guard: nothing but this file ever writes it, and this
 * file only writes it from the same thread that reads it. The cost is that the
 * count stops during an AdamNet transfer, when nobody is calling plat_vsync().
 * That is what the half-hour resync in tick.c is for.
 *
 * Thirty-two bits at 60 Hz is 828 days, so the wrap the CoCo backend has to
 * extend around has no counterpart here.
 */

#include <intrinsic.h>

#include "../gmail.h"
#include "platform.h"

static unsigned long frames;

void plat_vsync(void)
{
    intrinsic_halt();
    frames++;
}

unsigned long plat_ticks(void)
{
    return frames;
}

/*
 * The Adam is NTSC. Unlike the CoCo, which shipped in both and has no cheap
 * way to ask a 6847 which it is, there is no PAL variant to get wrong here.
 */
unsigned char plat_fps(void)
{
    return 60;
}
