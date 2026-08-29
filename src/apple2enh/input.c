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

    case 'r': case 'R':             return K_REFRESH;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

/*
 * Blocking, and blocking is all this client ever needs.
 *
 * The calendar backend spins through plat_vsync() here because it keeps its own
 * frame counter and a wall clock that has to advance while a screen waits.
 * A mailbox has neither: the date column is resolved once at boot and nothing
 * on screen changes until a key arrives.
 */
unsigned char plat_getkey(void)
{
    unsigned char c;

#ifdef GM_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    for (;;) {
        c = rawkey();
        if (c)
            return map(c);
    }
}

void plat_anykey(void)
{
    while (!rawkey())
        ;
}
