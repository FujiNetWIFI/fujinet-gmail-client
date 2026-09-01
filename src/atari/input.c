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

    /* R is K_REPLY everywhere and the inbox folds it into refresh; the
       CLR key stays refresh outright. */
    case 'r': case 'R':             return K_REPLY;
    case CH_CLR:                    return K_REFRESH;
    case 'c': case 'C':             return K_COMPOSE;
    case 'f': case 'F':             return K_FORWARD;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

void plat_anykey(void)
{
    plat_key();
}

/*
 * The compose form's read. The bare - = + * arrow aliases deliberately do
 * NOT apply here: inside a form they are text -- an address can carry any
 * of them -- so field movement is the real Ctrl-arrows only. ATASCII
 * $20-$7A is the range that coincides with printable ASCII (plus the $60
 * cell, whose byte is ASCII's backtick anyway); $7E is the backspace the
 * BACK S key sends.
 */
unsigned char plat_getch(void)
{
#ifdef GM_FAKE_KEYS
    /* Spent verbatim: the scripted K_* codes' low values land on E_* inside
       the form, which is what lets a capture drive the field cursor. */
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    for (;;) {
        unsigned char c = plat_key();

        switch (c) {
        case CH_CURS_UP:    return E_UP;
        case CH_CURS_DOWN:  return E_DOWN;
        case CH_CURS_LEFT:  return E_LEFT;
        case CH_CURS_RIGHT: return E_RIGHT;
        case CH_ENTER:      return E_ENTER;
        case CH_ESC:        return E_DONE;
        case 0x7E:          return E_BS;
        }

        if (c >= 0x20 && c <= 0x7A)
            return c;
    }
}
