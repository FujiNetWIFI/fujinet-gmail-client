/*
 * Frame timing.
 *
 * The H89's clock interrupt counts a 16-bit word every 2 ms -- at 000BH
 * under the Heath CP/M BIOS, 201BH under HDOS (con.h's TICKS) -- whatever
 * the program is doing, network calls included. The core wants frames, so a
 * frame here is ten ticks: 50 a second, which plat_fps() says.
 *
 * The word wraps every 131 seconds, so it is folded into the frame count
 * whenever anyone asks -- every key loop does, through plat_vsync(), and so
 * does each side of a bus call, the longest of which is the 90 s open. The
 * remainder is kept rather than thrown away, so the fold can be done as
 * often as anyone likes without the count running slow.
 */

#include "../gmail.h"
#include "platform.h"
#include "con.h"

#define TICKS_PER_FRAME 10

static unsigned long  frames;
static uint16_t       last;
static unsigned char  rem;
static unsigned char  started;

/* Sixteen bits, said so: the wrap is the counter's, not the compiler's. */
static void fold(void)
{
    uint16_t now = TICKS;
    uint16_t d;

    if (!started) {
        last = now;
        started = 1;
        return;
    }

    d = (uint16_t) (now - last);
    last = now;

    frames += d / TICKS_PER_FRAME;
    rem = (unsigned char) (rem + d % TICKS_PER_FRAME);
    if (rem >= TICKS_PER_FRAME) {
        rem -= TICKS_PER_FRAME;
        frames++;
    }
}

/* Wait for the next frame: what every key wait turns round, so the wall
   clock goes on counting while a screen sits there. */
void plat_vsync(void)
{
    unsigned long f;

    fold();
    f = frames;
    do
        fold();
    while (frames == f);
}

unsigned long plat_ticks(void)
{
    fold();
    return frames;
}

unsigned char plat_fps(void)
{
    return 500 / TICKS_PER_FRAME;
}

/* Nothing to suppress -- the card is polled, and the clock interrupt is the
   OS's -- but a fetch can be long, so fold the counter on each side of it. */
void plat_net_begin(void)
{
    fold();
}

void plat_net_end(void)
{
    fold();
}
