/*
 * Keyboard mapping.
 *
 * The Atari's cursor keys need Ctrl held down, which is a lot to ask of
 * someone browsing a mailbox, so the bare keycaps those arrows live on are
 * accepted too: - = + * read the same as up, down, left, right.
 */

#include <atari.h>

#include "../gmail.h"
#include "platform.h"

unsigned char plat_key(void);           /* key.s */

#ifdef GM_FAKE_KEYS
/*
 * Scripted input for headless testing. Build with, for example,
 *   -DGM_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER"
 * and the program drives itself that far, then falls through to the real
 * blocking read -- which is where a debugger breakpoint on plat_key catches it
 * with the screen of interest already painted.
 */
static const unsigned char fake_keys[] = { GM_FAKE_KEYS };
static unsigned char fake_idx;
#endif

unsigned char plat_getkey(void)
{
#ifdef GM_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    switch (plat_key()) {
    case CH_CURS_UP:    case '-':   return K_UP;
    case CH_CURS_DOWN:  case '=':   return K_DOWN;
    case CH_CURS_LEFT:  case '+':   return K_LEFT;
    case CH_CURS_RIGHT: case '*':   return K_RIGHT;

    case CH_ENTER:                  return K_ENTER;
    case CH_ESC:                    return K_BACK;

    case 'r': case 'R': case CH_CLR: return K_REFRESH;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

void plat_anykey(void)
{
    plat_key();
}
