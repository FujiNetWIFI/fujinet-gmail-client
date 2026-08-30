/*
 * Keyboard.
 *
 * There is no ESC key on a Color Computer, so BREAK is the back-out key -- the
 * same choice fujinet-config, fujinet-fujirkle and the calendar client all
 * made, and what the footer spells as BRK. CLEAR joins R as refresh, mirroring
 * the Atari's use of its own CLEAR.
 *
 * The Atari's ESC does double duty: back out of the reader, and quit from the
 * inbox. Splitting those is not a concession, it is an improvement -- BREAK
 * backs out and Q quits, so the key that leaves the program is never one
 * keystroke away from the key that leaves a message. BREAK is simply inert in
 * the inbox, which is what main.c's switch already does with an unhandled code.
 */

#include <coco.h>

#include "../gmail.h"
#include "platform.h"

#define KEY_LEFT    0x08
#define KEY_RIGHT   0x09
#define KEY_DOWN    0x0A
#define KEY_UP      0x5E
#define KEY_ENTER   0x0D
#define KEY_BREAK   0x03
#define KEY_CLEAR   0x0C

/* Color BASIC's 60 Hz frame counter. */
#define TIMER       (*(unsigned int *) 0x0112)

#ifdef GM_FAKE_KEYS
/*
 * Scripted input for headless testing. Build with, for example,
 *   -DGM_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER"
 * and the program drives itself that far, then falls through to the real
 * blocking read -- which is where a capture catches it with the screen of
 * interest already painted.
 */
static const unsigned char fake_keys[] = { GM_FAKE_KEYS };
static unsigned char fake_idx;
#endif

static unsigned char map(unsigned char c)
{
    switch (c) {
    case KEY_UP:                        return K_UP;
    case KEY_DOWN:                      return K_DOWN;
    case KEY_LEFT:                      return K_LEFT;
    case KEY_RIGHT:                     return K_RIGHT;

    case KEY_ENTER:                     return K_ENTER;
    case KEY_BREAK:                     return K_BACK;

    case 'r': case 'R': case KEY_CLEAR: return K_REFRESH;
    case 'q': case 'Q':                 return K_QUIT;
    }

    return K_NONE;
}

/*
 * One frame.
 *
 * CMOC accepts `volatile` and then ignores it, so this reads TIMER through the
 * macro on every pass rather than hoisting it into a register. The compiler
 * does no cross-statement elimination of pointer loads, which is what makes
 * that safe -- but it is the reason the loop is written this way and not with
 * a cached copy.
 *
 * See platform.h for why the wait exists at all in a program with no clock.
 */
void plat_vsync(void)
{
    unsigned int t = TIMER;

    while (TIMER == t)
        ;
}

/*
 * The blocking wait, kept in a function of its own on purpose.
 *
 * It polls inkey() around plat_vsync() rather than calling CMOC's waitkey(),
 * so that the program comes to rest inside a tight loop of *our* code. That is
 * where tools/coco-shot.sh sets its breakpoint: the scripted keys are consumed
 * before anything reaches here, so the first entry is exactly the moment the
 * script runs out and the screen under test is painted. Blocking in the ROM
 * instead would leave the PC in the BASIC keyboard scan, where no symbol names
 * it and a sampled capture has nothing to aim at.
 */
static unsigned char key_block(void)
{
    unsigned char c;

    for (;;) {
        c = inkey();
        if (c)
            return c;
        plat_vsync();
    }
}

unsigned char plat_getkey(void)
{
#ifdef GM_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    return map(key_block());
}

/*
 * Blocks unconditionally, scripted keys or not -- which is what the Atari and
 * the Apple backends both do, and it is the more useful behaviour rather than
 * merely the consistent one.
 *
 * The screens that call this are the ones that report a failure: not found, and
 * every error. Stopping dead on them means a capture photographs the failure
 * with the screen painted, instead of spending a scripted keystroke to walk past
 * the one thing worth looking at.
 */
void plat_anykey(void)
{
    key_block();
}
