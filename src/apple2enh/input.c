/*
 * Keyboard.
 *
 * The Atari needs Ctrl held down for its cursor keys, which is a lot to ask
 * while browsing a mailbox, so that backend accepts the bare keycaps those
 * arrows live on as well. An enhanced //e has real arrow keys and this one does
 * not need the workaround.
 *
 * Read straight off the hardware rather than through cgetc(), for the same
 * reason the Atari calls KEYBDV directly: cc65's read maintains a cursor, and
 * a cursor writes over cells this program painted itself.
 */

#include <apple2.h>
#include <peekpoke.h>

#include "../gmail.h"
#include "platform.h"

#define KBD         0xC000      /* bit 7 set while a key is waiting */
#define KBDSTRB     0xC010

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

/* The waiting key, stripped of the strobe bit, or 0 for nothing. */
static unsigned char rawkey(void)
{
    unsigned char c = PEEK(KBD);

    if (!(c & 0x80))
        return 0;

    POKE(KBDSTRB, 0);

    return (unsigned char) (c & 0x7F);
}

static unsigned char map(unsigned char c)
{
    switch (c) {
    case CH_CURS_UP:                return K_UP;
    case CH_CURS_DOWN:              return K_DOWN;
    case CH_CURS_LEFT:              return K_LEFT;
    case CH_CURS_RIGHT:             return K_RIGHT;

    case CH_ENTER:                  return K_ENTER;
    case CH_ESC:                    return K_BACK;

    /* R is K_REPLY everywhere; the inbox folds it into refresh. */
    case 'r': case 'R':             return K_REPLY;
    case 'c': case 'C':             return K_COMPOSE;
    case 'f': case 'F':             return K_FORWARD;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

/*
 * The wait every blocking read is built on.
 *
 * This machine has no free-running counter of its own, so the wall clock runs
 * on frames counted in plat_vsync() -- which means the wait has to be a poll
 * around it rather than a spin on the keyboard alone. The cost is one
 * vertical-retrace wait per pass; the gain is a clock that is still right when
 * the user comes back to the screen.
 */
static unsigned char key_wait(void)
{
    unsigned char c;

    for (;;) {
        c = rawkey();
        if (c)
            return c;

        plat_vsync();
        clock_pump();
    }
}

unsigned char plat_getkey(void)
{
#ifdef GM_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    return map(key_wait());
}

void plat_anykey(void)
{
    key_wait();
}

/*
 * The compose form's read. DELETE ($7F) erases; the left arrow ($08) moves
 * the cursor, which is what it does in every Apple editor -- erasing is
 * what it means in BASIC, but this machine has a DELETE key and BASIC does
 * not get a vote on a form. Polls through key_wait() like plat_getkey(), and
 * for the same reason: this is the screen a user sits on longest, so it is the
 * one whose clock going stale would be most obvious.
 */
unsigned char plat_getch(void)
{
    unsigned char c;

#ifdef GM_FAKE_KEYS
    /* Spent verbatim: the scripted K_* codes' low values land on E_* inside
       the form, which is what lets a capture drive the field cursor. */
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    for (;;) {
        c = key_wait();

        switch (c) {
        case CH_CURS_UP:        return E_UP;
        case CH_CURS_DOWN:      return E_DOWN;
        case CH_CURS_LEFT:      return E_LEFT;
        case CH_CURS_RIGHT:     return E_RIGHT;
        case CH_ENTER:          return E_ENTER;
        case CH_ESC:            return E_DONE;
        case 0x7F:              return E_BS;
        }

        if (c >= 0x20 && c < 0x7F)
            return c;
    }
}
