/*
 * Keyboard and SmartKeys.
 *
 * The Adam has six labelled keys above the keyboard whose captions are drawn on
 * the screen, which is a better answer to "what do the keys do" than any of the
 * other three backends could give. The Atari spends its bottom row on
 * "RET:READ  <>:PAGE  R:REFRESH  ESC:QUIT", the CoCo on the same thing
 * abbreviated to fit thirty-two columns, the Apple on a hint bar it
 * deliberately leaves in normal video. Here the machine has a place to put it,
 * and rows 0-20 stay content -- which is the row the list gets back.
 *
 * The core only knows K_* codes, so a SmartKey means whatever the screen
 * currently on display says it means. Legend and meaning are therefore set
 * together, by sk_bind(), and never separately: a legend that has drifted from
 * its map is a key that lies about what it does.
 *
 * The letters and arrows are kept as well, and match the other backends exactly
 * -- RETURN reads, R refreshes, Q quits. A SmartKey is discoverable and a
 * keystroke is fast, and there is no reason to make anyone choose.
 */

#include <eos.h>
#include <intrinsic.h>
#include <smartkeys.h>

#include "../gmail.h"
#include "platform.h"

/* EOS keyboard codes. eoslib's eos_read_keyboard_special_keys.md is the
   reference; the arrows are a contiguous run in the order up, right, down,
   left. */
#define KEY_RETURN      0x0D
#define KEY_ESCAPE      0x1B
#define KEY_UNDO        0x91
#define KEY_CLEAR       0x96
#define KEY_UP          0xA0
#define KEY_RIGHT       0xA1
#define KEY_DOWN        0xA2
#define KEY_LEFT        0xA3

/* What the six keys currently mean. Set as a block by sk_bind(), so a screen
   that binds fewer than six does not inherit the previous screen's. */
static unsigned char sk_key[6];

/* The set on display. smartkeys_display() clears and repaints all three band
   rows, which is far too much work to do on every ui_inbox_sel(), and nothing
   in this backend ever paints over rows 21-23 -- so if the pointer has not
   changed, the legend on screen is still right. */
static const struct sk_set *sk_cur;

#ifdef GM_FAKE_KEYS
/*
 * Scripted input for headless testing, the same mechanism the CoCo backend
 * carries. Build with -DGM_FAKE_KEYS="K_DOWN,K_DOWN,K_ENTER" and the program
 * drives itself that far, then falls through to the real blocking read so the
 * screen under test stays painted for the capture.
 */
static const unsigned char fake_keys[] = { GM_FAKE_KEYS };
static unsigned char fake_idx;
#endif

/* ------------------------------------------------------------------ */
/* SmartKeys                                                           */
/* ------------------------------------------------------------------ */

void sk_bind(const struct sk_set *s)
{
    unsigned char i;

    for (i = 0; i < 6; i++)
        sk_key[i] = s->key[i];

    if (sk_cur == s)
        return;
    sk_cur = s;

    smartkeys_display(s->label[0], s->label[1], s->label[2],
                      s->label[3], s->label[4], s->label[5]);
}

/* ------------------------------------------------------------------ */
/* Translation                                                         */
/* ------------------------------------------------------------------ */

static unsigned char map(unsigned char c)
{
    if (c >= SMARTKEY_I && c <= SMARTKEY_VI)
        return sk_key[c - SMARTKEY_I];

    switch (c) {
    case KEY_UP:                        return K_UP;
    case KEY_DOWN:                      return K_DOWN;
    case KEY_LEFT:                      return K_LEFT;
    case KEY_RIGHT:                     return K_RIGHT;

    case KEY_RETURN:                    return K_ENTER;
    case KEY_ESCAPE:
    case KEY_UNDO:                      return K_BACK;

    case 'r': case 'R': case KEY_CLEAR: return K_REFRESH;
    case 'q': case 'Q':                 return K_QUIT;
    }

    return K_NONE;
}

/*
 * One raw key, or 0 if none is waiting.
 *
 * EOS reads the keyboard in the background: eos_start_read_keyboard() arms a
 * read and eos_end_read_keyboard() answers 0 or 1 until one completes, then
 * hands back the code. Every completed read has to be re-armed or the keyboard
 * goes quiet, which is why that call is here rather than at init only.
 */
static unsigned char raw(void)
{
    unsigned char c = eos_end_read_keyboard();

    if (c <= 1)
        return 0;

    eos_start_read_keyboard();
    return c;
}

/*
 * One frame.
 *
 * A HALT rather than a counter: the VDP raises an NMI once per frame and
 * z88dk's coleco crt installs a handler for it unconditionally, so HALT wakes
 * once per frame with no interrupt of our own to install and no multi-byte
 * counter to read twice. It also keeps the key poll off the AdamNet bus for
 * sixteen milliseconds at a time, which a bare spin on eos_end_read_keyboard()
 * would not.
 *
 * That it wakes at all is worth having checked rather than reasoned about, and
 * it has been: counting the wakes into a global and reading it out of two
 * ADAMEm snapshots twenty seconds apart gave 1204 frames, which is 60.2 Hz --
 * the NTSC rate, so every HALT is being ended by the VDP and none of them by
 * anything else. The calendar client reaches the same place via
 * add_raster_int(), and needs to only because its HAL exposes a tick count.
 */
void plat_vsync(void)
{
    intrinsic_halt();
}

/* ------------------------------------------------------------------ */
/* The plat_ contract                                                  */
/* ------------------------------------------------------------------ */

unsigned char plat_getkey(void)
{
    unsigned char c, k;

#ifdef GM_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys))
        return fake_keys[fake_idx++];
#endif

    for (;;) {
        c = raw();
        if (c) {
            k = map(c);
            if (k != K_NONE)
                return k;
        }
        plat_vsync();
    }
}

/*
 * Any key at all, including the ones map() throws away -- this is the "press
 * any key" of the error and not-found screens, and a user who presses SmartKey
 * III there means to continue.
 */
void plat_anykey(void)
{
#ifdef GM_FAKE_KEYS
    if (fake_idx < sizeof(fake_keys)) {
        fake_idx++;
        return;
    }
#endif

    while (!raw())
        plat_vsync();
}
