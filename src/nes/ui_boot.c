/*
 * ui_boot.c -- the BOOT bank: setting the machine up, and the flat screens
 * (splash, not found, busy, error, sent).
 *
 * Setting up is the screen's skeleton, which never changes after this:
 *   - every bitmap tile's plane 1 to $FF and planes 0 clear, so the whole
 *     text screen is paper until something is drawn on it;
 *   - name table rows 2-29 numbering their cells band by band (row r, column
 *     x shows tile (r & 7) * 32 + x of its band's table), rows 0-1 the blank
 *     fixed tile $80 above the text;
 *   - the palettes, the sprites' patterns, and the keyboard, if there is one.
 *
 * The flat screens are Gmail's sign-in page, more or less: white, the mark
 * large in the middle, a line or two of text under it, and Google's four
 * dots bobbing while the FujiNet works.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"

#pragma code-name (push, "BOOT")
#pragma rodata-name (push, "BOOTRO")

/*
 * Background: 0 page (white, black ink), 1 selection (Gmail's pale blue), 2
 * read mail and quiet text (white, grey ink), 3 the app bars (Gmail red,
 * white). Sprites: 0 the mark's left half and the blue and red dots, 1 its
 * right half and the yellow and green, 2 the white behind the header's mark,
 * 3 the form's cursor. Colour 0 is white throughout, which is also what the
 * screen shows while it is off.
 */
static const uint8_t palette[32] = {
    0x30, 0x30, 0x30, 0x0F,
    0x30, 0x30, 0x31, 0x0F,
    0x30, 0x30, 0x30, 0x00,
    0x30, 0x30, 0x16, 0x30,
    0x30, 0x11, 0x16, 0x06,
    0x30, 0x1A, 0x16, 0x28,
    0x30, 0x30, 0x10, 0x0F,
    0x30, 0x11, 0x16, 0x0F
};

/* Fill CHR bank `bank` (1K) through R2's window at $1000: as bitmap tiles
   (plane 0 clear, plane 1 $FF) or all clear. Screen off. */
static void chr_fill(uint8_t bank, uint8_t bitmap)
{
    uint8_t t, v = bitmap ? 0xFF : 0x00;

    MMC3_SEL = 2;
    MMC3_BANK = bank;
    MMC3_SEL = mmc3_sel;
    PPU_ADDR = 0x10;
    PPU_ADDR = 0x00;
    for (t = 0; t < 64; ++t) {
        PPU_DATA = 0; PPU_DATA = 0; PPU_DATA = 0; PPU_DATA = 0;
        PPU_DATA = 0; PPU_DATA = 0; PPU_DATA = 0; PPU_DATA = 0;
        PPU_DATA = v; PPU_DATA = v; PPU_DATA = v; PPU_DATA = v;
        PPU_DATA = v; PPU_DATA = v; PPU_DATA = v; PPU_DATA = v;
    }
}

void boot_init(void)
{
    uint8_t b, r, x;

    /* crt0.s left the screen off (ppu_direct, ppu_off). */
    for (b = 0; b < CHR_FIXED; ++b)
        chr_fill(b, 1);
    for (b = CHR_FIXED; b < CHR_BODY; ++b)
        chr_fill(b, 0);
    MMC3_SEL = 2;
    MMC3_BANK = CHR_SPR;
    MMC3_SEL = mmc3_sel;

    PPU_ADDR = 0x20;
    PPU_ADDR = 0x00;
    for (x = 0; x < 64; ++x)
        PPU_DATA = 0x80;
    for (r = 0; r < SCR_ROWS; ++r)
        for (x = 0; x < SCR_TILES; ++x)
            PPU_DATA = (uint8_t) (((r & 7) << 5) + x);
    for (x = 0; x < 64; ++x)
        PPU_DATA = 0;

    memcpy(pal, palette, sizeof(pal));
    pal_dirty = 1;
    att_dirty = 0xFF;
    for (r = 0; r < SCR_ROWS; ++r) {
        dirty_lo[r] = 0xFF;
        dirty_hi[r] = 0;
    }
    memset(scr_txt, ' ', sizeof(scr_txt));

    spr_init();
    inp_init();
}

/* ------------------------------------------------------------------ */
/* The flat screens                                                    */
/* ------------------------------------------------------------------ */

#define FLAT_MARK   4           /* rows 4-7 */
#define FLAT_HEAD   10
#define FLAT_BODY   12
#define FLAT_SPIN   16
#define FLAT_CODES  22
#define FLAT_FOOT   26


static void error_text(uint8_t code, const char **l1, const char **l2)
{
    *l2 = 0;
    switch (code) {
    case GM_NOAUTH:
        *l1 = "This FujiNet is not signed in to Google yet.";
        *l2 = "Authorize Gmail in the FujiNet's web page, then try again.";
        break;
    case GM_DENIED:
        *l1 = "Google refused access to the mailbox.";
        *l2 = "Authorize Gmail again in the FujiNet's web page.";
        break;
    case GM_NOTFOUND:
        *l1 = "That message is no longer there.";
        *l2 = "Refresh the inbox and try again.";
        break;
    case GM_REJECTED:
        *l1 = "Gmail would not take the message.";
        *l2 = "Check the address in To.";
        break;
    case GM_TOOBIG:
        *l1 = "The message is too large to send.";
        break;
    case GM_NOSERVICE:
        *l1 = "Gmail could not be reached.";
        *l2 = "Check the FujiNet's network connection.";
        break;
    case 0:
        *l1 = "The FujiNet did not answer in time.";
        break;
    default:
        strcpy(ui_buf, "Error ");
        utoa(code, ui_buf + 6, 10);
        *l1 = ui_buf;
        break;
    }
}

void boot_flat(uint8_t what, uint8_t code, const char *stage)
{
    const char *l1, *l2 = 0;
    char        n[6];

    ui_screen = SCREEN_FLAT;
    header_off();
    scr_begin();
    spr_clear();
    scr_cls();
    spr_mark_big(FLAT_MARK);

    switch (what) {
    case FLAT_SPLASH:
        scr_center(FLAT_HEAD, "Gmail");
        scr_band(FLAT_BODY / 2, PAL_GREY);
        scr_center(FLAT_BODY, "Looking for the FujiNet");
        spr_spin(FLAT_SPIN);
        break;
    case FLAT_NOTFOUND:
        scr_center(FLAT_HEAD, "No FujiNet found");
        scr_center(FLAT_BODY, "Check the cartridge and its WiFi.");
        scr_center(FLAT_FOOT, "Press any button to look again");
        break;
    case BUSY_INDEX:
        scr_center(FLAT_HEAD, "Opening your inbox");
        scr_band(FLAT_BODY / 2, PAL_GREY);
        scr_center(FLAT_BODY, "This can take up to a minute.");
        spr_spin(FLAT_SPIN);
        break;
    case BUSY_BODY:
        scr_center(FLAT_HEAD, "Opening the message");
        spr_spin(FLAT_SPIN);
        break;
    case BUSY_SEND:
        scr_center(FLAT_HEAD, "Sending");
        spr_spin(FLAT_SPIN);
        break;
    case FLAT_SENT:
        scr_center(FLAT_HEAD, "Message sent");
        scr_center(FLAT_FOOT, "Press any button");
        break;
    case FLAT_ERROR:
        error_text(code, &l1, &l2);
        scr_band(FLAT_HEAD / 2, PAL_RED);
        scr_center(FLAT_HEAD, "Something went wrong");
        scr_center(FLAT_BODY, l1);
        if (l2)
            scr_center(FLAT_BODY + 1, l2);

        /* The raw codes under the friendly words: on real hardware this is
           the difference between a reportable bug and "it said error". */
        strcpy(ui_buf, stage);
        strcat(ui_buf, " code ");
        utoa(code, n, 10);
        strcat(ui_buf, n);
        strcat(ui_buf, " dev ");
        utoa(gm_dev_ecode, n, 10);
        strcat(ui_buf, n);
        scr_band(FLAT_CODES / 2, PAL_GREY);
        scr_center(FLAT_CODES, ui_buf);
        scr_center(FLAT_FOOT, "Press any button");
        break;
    }

    scr_end();
}

