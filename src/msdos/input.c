/*
 * Keyboard.
 *
 * INT 16h AH=00 blocks until a key and hands back ASCII in AL with the scan
 * code in AH. The arrows and the grey navigation keys have no ASCII: the
 * 83-key board returns AL=0 for them, the 101-key returns AL=E0h, and both
 * put the scan code in AH -- so anything with an empty AL is mapped by scan
 * code and everything else by character.
 *
 * PgUp and PgDn alias the page keys. They are free on this keyboard, they
 * are what a DOS user's fingers already do in a reader, and on the PCjr --
 * where the arrows themselves need the Fn shift -- they are no worse than
 * anything else.
 *
 * The Atari's ESC does double duty: back out of the reader, and quit from
 * the inbox. This backend takes the CoCo's split instead -- ESC backs out,
 * Q quits -- so leaving the program is never one keystroke from the key
 * that leaves a message. ESC is simply inert in the inbox, and the footer
 * says Q:QUIT because that is what actually quits.
 */

#include <dos.h>
#ifdef GM_SHOT
#include <stdlib.h>
#endif

#include "../gmail.h"
#include "platform.h"

/*
 * Scripted input for headless testing. Build with, for example,
 *   -DGM_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER"
 * and the program drives itself that far, then falls through to the real
 * blocking read -- which is where a capture catches it with the screen of
 * interest already painted.
 *
 * The second spelling is this compiler's own problem: wcc cannot carry a
 * comma through -D -- everything after it is parsed as another file to
 * compile, E1139 -- so a *sequence* has to arrive without one.
 * GM_FAKE_KEYS_STR is the K_* codes as a string of digits ('2' is K_DOWN),
 * which rides through the same escaped-quote path GIT_VERSION already
 * proves out. tools/msdos-shot.sh does the translation, so nobody types
 * digits by hand.
 */
#ifdef GM_FAKE_KEYS
static const unsigned char fake_keys[] = { GM_FAKE_KEYS };
static unsigned char fake_idx;
#elif defined(GM_FAKE_KEYS_STR)
static const char fake_keys[] = GM_FAKE_KEYS_STR;
static unsigned char fake_idx;
#endif

#define SC_UP       0x48
#define SC_DOWN     0x50
#define SC_LEFT     0x4B
#define SC_RIGHT    0x4D
#define SC_PGUP     0x49
#define SC_PGDN     0x51

/* Blocking BIOS read: AL the character, AH the scan code. */
static unsigned int rawkey(void)
{
    union REGS r;

    r.h.ah = 0x00;
    int86(0x16, &r, &r);

    return r.x.ax;
}

static unsigned char map(unsigned int ax)
{
    unsigned char al = (unsigned char) (ax & 0xFF);

    if (al == 0x00 || al == 0xE0) {
        switch ((unsigned char) (ax >> 8)) {
        case SC_UP:                 return K_UP;
        case SC_DOWN:               return K_DOWN;
        case SC_LEFT:               return K_LEFT;
        case SC_RIGHT:              return K_RIGHT;
        case SC_PGUP:               return K_LEFT;
        case SC_PGDN:               return K_RIGHT;
        }
        return K_NONE;
    }

    switch (al) {
    case 0x0D:                      return K_ENTER;
    case 0x1B:                      return K_BACK;
    case 'r': case 'R':             return K_REFRESH;
    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

/*
 * Blocking, and blocking is all this client ever needs.
 *
 * The calendar backend spins through plat_vsync() here because it keeps its
 * own frame counter and a wall clock that has to advance while a screen
 * waits. A mailbox has neither: the date column is resolved once at boot and
 * nothing on screen changes until a key arrives.
 */
unsigned char plat_getkey(void)
{
#ifdef GM_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#elif defined(GM_FAKE_KEYS_STR)
    if (fake_keys[fake_idx])
        return (unsigned char) (fake_keys[fake_idx++] - '0');
#endif

#ifdef GM_SHOT
    /* A capture run ends where a person would start: the screen of interest
       is painted and the program is about to block. Dump it and leave. */
    scr_snapshot();
    exit(0);
#endif

    for (;;) {
        unsigned char k = map(rawkey());
        if (k != K_NONE)
            return k;
    }
}

void plat_anykey(void)
{
#ifdef GM_SHOT
    scr_snapshot();
    exit(0);
#endif
    rawkey();
}
