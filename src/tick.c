/*
 * The wall clock between device reads.
 *
 * src/clock.c asks the FujiNet what time it is; this advances that answer off
 * the machine's own frame counter until the next time it asks. Pure arithmetic
 * over two platform calls, which is what lets tests/hosttest.c drive it a day
 * at a time instead of waiting one.
 *
 * Ported from the calendar client's src/clock.c, which had the same problem
 * first. Two things are simpler here. A mailbox has no alarms, so only the
 * minute is worth repainting and the second is only ever an accumulator. And
 * nothing on screen carries the date, so midnight is not something to compute
 * -- it is just a reason to ask the device again, which is cheap once a day.
 *
 * The drift this has to survive is the transport: nobody pumps the counter
 * while fujinet-lib is talking to the device, and on the machines whose tick
 * is maintained by an interrupt the transport masks it outright. That is what
 * the half-hour resync is for, and why the answer it resyncs to is the
 * FujiNet's and not this file's.
 */

#include "gmail.h"

#define RESYNC_MINUTES  30

unsigned char gm_h, gm_mi, gm_s;
unsigned char gm_clock_ok;

static unsigned long last;      /* tick count the current second started at */
static unsigned char since;     /* minutes since the last good device read */
static unsigned char fps;

void tick_reset(void)
{
    /* Asked every time rather than cached here. It cannot change while the
       program runs, but the backends that pay anything to answer it already
       cache it themselves, so a cache here would only be a second copy of the
       same fact -- and one the tests could not reach past. */
    fps = plat_fps();

    last = plat_ticks();
    since = 0;
}

unsigned char tick_due_resync(void)
{
    return (unsigned char) (since >= RESYNC_MINUTES);
}

/*
 * Returns 1 when the displayed minute changed, which is the only thing worth
 * repainting -- and, on a 6502 painting through a display list, the difference
 * between a clock and a flicker.
 */
unsigned char tick_advance(void)
{
    unsigned long now;
    unsigned int  secs;
    unsigned char bumped = 0;

    if (!gm_clock_ok)
        return 0;

    now = plat_ticks();
    if (now < last) {
        /* The counter wrapped. Everything since the last call is lost, which
           is at most one frame: this runs every time round a key wait. */
        last = now;
        return 0;
    }

    secs = (unsigned int) ((now - last) / fps);
    if (secs == 0)
        return 0;

    /* An hour is long enough that stepping second by second is silly, and long
       enough that the FujiNet's own clock is the better answer anyway. */
    if (secs > 3600) {
        since = RESYNC_MINUTES;
        return 1;
    }

    /* Advance the baseline by whole seconds only, so the remainder carries
       rather than being thrown away once a second. */
    last += (unsigned long) secs * fps;

    while (secs--) {
        if (++gm_s < 60)
            continue;
        gm_s = 0;
        bumped = 1;

        if (since < 255)
            since++;

        if (++gm_mi < 60)
            continue;
        gm_mi = 0;

        if (++gm_h < 24)
            continue;
        gm_h = 0;
        since = RESYNC_MINUTES;         /* midnight: ask, do not compute */
    }

    return bumped;
}

/*
 * The hook every backend's blocking key wait calls, once round the loop. One
 * frame's worth of clock, and a repaint only when the minute on screen has
 * actually gone stale.
 */
void clock_pump(void)
{
    if (tick_advance())
        ui_clock();
}
