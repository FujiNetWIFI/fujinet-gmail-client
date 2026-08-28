/*
 * Color band control.
 *
 * We do not build a display list. The OS already has a perfectly good 24-row
 * GRAPHICS 0 list, correctly aligned, with its LMS pointing at the screen
 * memory conio writes to -- all we need is the interrupt bit set on two of its
 * mode bytes. Poking two bits is far less to get wrong than owning a display
 * list, and it keeps clrscr() and friends working untouched.
 */

#include <atari.h>

#include "../gmail.h"
#include "platform.h"

/* dli.s */
extern unsigned char dli_list_bg, dli_list_fg;
extern unsigned char dli_foot_bg, dli_foot_fg;
void dli_hw_on(void);
void dli_hw_off(void);
void dli_vbi_install(void);
void dli_vbi_remove(void);

/*
 * Display list layout for 24-row GRAPHICS 0, 32 bytes:
 *
 *   0..2   $70 $70 $70    24 blank scanlines
 *   3      $42            LMS + ANTIC mode 2   -> text row 0
 *   4..5   screen address
 *   6      $02            -> text row 1
 *   ...
 *   28     $02            -> text row 23
 *   29..31 $41 + address  JVB
 *
 * So row 0 is at offset 3 and every row after it is at offset 5 + row. A DLI
 * fires on the last scanline of the row whose bit is set, and its writes land
 * on the row after -- hence the bit goes on the row *before* each band.
 *
 * Only rows 1 and up ever carry an interrupt bit here, so the macro does not
 * bother with the row 0 special case.
 */
#define DL_ROW(r)   (5 + (r))

static unsigned char bands_wanted;      /* current screen wants banding */
static unsigned char bands_armed;       /* the DL actually carries DLI bits */

static void poke_dli_bits(unsigned char on)
{
    unsigned char *dl = (unsigned char *) OS.sdlst;

    bands_armed = 0;

    /* Only touch a display list we recognize. If the OS handed us something
       unexpected, fall back to flat color rather than corrupt it. */
    if (dl == 0 || dl[3] != 0x42)
        return;

    if (on) {
        dl[DL_ROW(HDR_ROWS - 1)] |= 0x80;       /* row 2  -> list colors */
        dl[DL_ROW(FOOT_ROW - 1)] |= 0x80;       /* row 22 -> footer colors */
    } else {
        dl[DL_ROW(HDR_ROWS - 1)] &= 0x7F;
        dl[DL_ROW(FOOT_ROW - 1)] &= 0x7F;
    }

    bands_armed = on;
}

void dli_bands(void)
{
    dli_list_bg = C_LIST_BG;
    dli_list_fg = C_LIST_FG;
    dli_foot_bg = C_FOOT_BG;
    dli_foot_fg = C_FOOT_FG;

    /* The vertical blank restores these every frame, which is exactly how the
       header band gets its color back at the top of each screen. */
    OS.color2 = C_HDR_BG;
    OS.color1 = C_HDR_FG;
    OS.color4 = C_BORDER;

    /* Already banded: the colours above are all that needed refreshing. Do not
       touch NMIEN -- ui_message() calls this on every repaint, and enabling
       interrupts part-way down a frame is what knocks the chain out of phase
       in the first place. */
    if (bands_wanted && bands_armed)
        return;

    bands_wanted = 1;
    poke_dli_bits(1);
    if (bands_armed) {
        waitvsync();            /* arm at the top of a frame, not mid-screen */
        dli_hw_on();
    }
}

void dli_flat(unsigned char bg, unsigned char fg)
{
    bands_wanted = 0;
    dli_hw_off();
    poke_dli_bits(0);

    OS.color2 = bg;
    OS.color1 = fg;
    OS.color4 = C_BORDER;
}

void dli_shutdown(void)
{
    bands_wanted = 0;
    dli_hw_off();
    poke_dli_bits(0);
    dli_vbi_remove();
}

/*
 * SIO runs with interrupts disabled and is timing critical. A DLI firing in
 * the middle of a transfer steals cycles it cannot spare, so every network and
 * fuji call is bracketed by these. Nothing is lost visually: the busy and
 * error screens are flat by design.
 */
void plat_net_begin(void)
{
    dli_hw_off();
}

void plat_net_end(void)
{
    if (bands_wanted && bands_armed) {
        waitvsync();            /* re-arm at the top of a frame, not mid-screen */
        dli_hw_on();
    }
}
