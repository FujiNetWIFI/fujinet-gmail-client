/*
 * Glyphs and colour on the GRAPHICS II page, plus bringing the machine up and
 * putting it back.
 *
 * The two planes are written by different means and that split runs through the
 * whole file. Glyphs go out through z88dk's console, which knows how to blit a
 * font cell into eight pattern bytes at an arbitrary position and is what every
 * other Adam FujiNet client uses. Colour is written straight into the attribute
 * plane with vdp_vfill, one call per run.
 *
 * Every glyph-writing call repaints the attribute run behind it. That is not
 * belt and braces: the console keeps its own notion of the current colour and
 * changes it whenever anything else prints -- smartkeys_display() sets it six
 * times -- so a field whose colour came from whatever vdp_color() was last
 * called with is a field whose colour is a function of paint order. Repainting
 * makes each field's appearance depend on its own arguments and nothing else.
 *
 * Nothing here touches rows 21-23. scr_clear() clears twenty-one rows rather
 * than calling clrscr(), because clrscr() would take the SmartKeys legend with
 * it and smartkeyslib would have to be asked to paint it again.
 *
 * Unlike the CoCo there is a full ASCII font here, so nothing folds case and
 * the string literals in this directory are written the way they read.
 */

#include <conio.h>
#include <eos.h>
#include <smartkeys.h>
#include <string.h>
#include <video/tms99x8.h>

#include "../gmail.h"
#include "platform.h"

/* One row, padded, plus the terminator. */
static char pad[SCR_COLS + 1];

/* ------------------------------------------------------------------ */
/* Clearing                                                            */
/* ------------------------------------------------------------------ */

void scr_clear(void)
{
    vdp_vfill(PAT_BASE, 0x00, OUR_BYTES);
    vdp_vfill(MODE2_ATTR, A_BODY, OUR_BYTES);
}

void scr_row_clear(unsigned char row)
{
    vdp_vfill(PAT_ADDR(row, 0), 0x00, 256);
    vdp_vfill(ATT_ADDR(row, 0), A_BODY, 256);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    unsigned int n;

    if (last < first)
        return;

    n = (unsigned int) (last - first + 1) << 8;

    vdp_vfill(PAT_ADDR(first, 0), 0x00, n);
    vdp_vfill(ATT_ADDR(first, 0), A_BODY, n);
}

/* ------------------------------------------------------------------ */
/* Colour                                                              */
/* ------------------------------------------------------------------ */

void scr_attr(unsigned char row, unsigned char col, unsigned char width,
              unsigned char attr)
{
    if (col >= SCR_COLS || width == 0)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    vdp_vfill(ATT_ADDR(row, col), attr, (unsigned int) width << 3);
}

/*
 * A solid block of colour: no lit pixels, and an attribute whose background is
 * the ink. Blanking the pattern matters -- a chip is often painted over a cell
 * that used to hold a glyph, and leaving the glyph would show it in black
 * against the chip.
 */
void scr_fill(unsigned char row, unsigned char col, unsigned char ink,
              unsigned char width)
{
    if (col >= SCR_COLS || width == 0)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    vdp_vfill(PAT_ADDR(row, col), 0x00, (unsigned int) width << 3);
    vdp_vfill(ATT_ADDR(row, col), A_BLOCK(ink), (unsigned int) width << 3);
}

void scr_cell(unsigned char row, unsigned char col, unsigned char ink)
{
    scr_fill(row, col, ink, 1);
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

/*
 * The console's cursor is left wherever the last character landed, so every
 * write positions it first. Writing the full width of row 20 leaves it at the
 * start of row 21, which is inside the SmartKeys band -- harmless only because
 * nothing in this backend ever prints without a gotoxy() in front of it.
 */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr)
{
    unsigned char n = 0;
    unsigned char c;

    if (col >= SCR_COLS || width == 0)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    while (n < width && *s) {
        c = (unsigned char) *s++;
        pad[n++] = (char) ((c < 0x20 || c > 0x7E) ? '?' : c);
    }
    while (n < width)
        pad[n++] = ' ';
    pad[n] = '\0';

    gotoxy(col, row);
    cputs(pad);

    scr_attr(row, col, width, attr);
}

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char attr)
{
    unsigned char len = (unsigned char) strlen(s);

    if (col >= SCR_COLS)
        return;
    if (col + len > SCR_COLS)
        len = (unsigned char) (SCR_COLS - col);
    scr_field(row, col, s, len, attr);
}

/* Right-align s so that its last character lands on column rcol. */
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char attr)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len > rcol + 1)
        len = (unsigned char) (rcol + 1);
    scr_field(row, (unsigned char) (rcol + 1 - len), s, len, attr);
}

void scr_center(unsigned char row, const char *s, unsigned char attr)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len >= SCR_COLS)
        scr_field(row, 0, s, SCR_COLS, attr);
    else
        scr_text(row, (unsigned char) ((SCR_COLS - len) / 2), s, attr);
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/*
 * The order here is load-bearing at both ends. smartkeys_set_mode() has to come
 * first because it calls vdp_set_mode(2), which rewrites every VDP register and
 * clears all of VRAM -- anything installed before it would be wiped, including
 * the sprite patterns and the sprite list terminator logo_init() writes. The
 * keyboard is armed last because EOS reads it in the background and the first
 * read has to be outstanding before the first poll.
 *
 * smartkeys_sound_init() is deliberately absent. This client has no chime, and
 * that call installs a raster interrupt that runs eos_play_sound() every frame
 * for the rest of the run.
 */
void plat_init(void)
{
    smartkeys_set_mode();

    logo_init();

    eos_start_read_keyboard();

    /*
     * The Adam comes up black on cyan. Gmail is black on white, and the border
     * matches the content ground rather than the app bar: the bar is two rows
     * and the content is nineteen, so a red border would frame the wrong thing.
     */
    vdp_color(VDP_INK_BLACK, VDP_INK_WHITE, VDP_INK_WHITE);
    scr_clear();
}

/*
 * Quitting hands the machine back to SmartWriter, which is the Adam's idea of
 * where a program goes when it is finished. There is nothing else to return to:
 * this build is linked at $0000 in all-RAM mode, so the boot block that loaded
 * it is long gone.
 */
void plat_shutdown(void)
{
    logo_hide();
    scr_clear();
    smartkeys_display(0, 0, 0, 0, 0, 0);
    eos_exit_to_smartwriter();
}

/*
 * Nothing to suppress. On the Atari these switch off display list interrupts,
 * which would otherwise steal cycles from a timing-critical SIO transfer.
 * AdamNet is a master/slave bus driven synchronously from inside EOS, and the
 * only interrupt running here is the VDP's NMI, which cannot be masked anyway.
 * They stay because they are the contract, and because they are where the fix
 * would go if a transfer ever did turn out to mind.
 */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
