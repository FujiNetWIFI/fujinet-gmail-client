/*
 * header.c -- what the inbox, the reader and the form share: the app bar
 * across the top, the hint bar across the bottom, and the clock.
 *
 * The app bar is rows 0-1 in Gmail red, white lettering, with the mark at
 * its left -- sprites on a white square of their own, because the bar's red
 * would swallow the mark's red stroke. "Gmail" sits beside it on row 0 with
 * the clock at the far right; row 1 carries the screen's title. The hint bar
 * is rows 26-27 in the same red: the controller's keys on row 26 and, when a
 * keyboard is plugged in, its keys on row 27.
 *
 * In the fixed half because every screen's bank calls it.
 */

#include "../gmail.h"
#include "platform.h"

#define HDR_TEXT_COL    6
#define CLOCK_END       63

uint8_t ui_screen;
static uint8_t clock_on;

/* Scratch for the painters. Only one screen is ever being painted, so the
   banks share these rather than each keeping its own in 8K of WRAM. */
char ui_buf[UI_BUF];
char ui_wrap[2][SCR_COLS + 1];
char ui_num[12];

void header_paint(const char *title)
{
    scr_band(0, PAL_RED);
    scr_put(0, 0, "", HDR_TEXT_COL);
    scr_put(0, HDR_TEXT_COL, "Gmail", (uint8_t) (CLOCK_END - 5 - HDR_TEXT_COL));
    scr_put(1, 0, "", HDR_TEXT_COL);
    scr_put(1, HDR_TEXT_COL, title, (uint8_t) (SCR_COLS - HDR_TEXT_COL));
    spr_mark();
    clock_on = 1;
    ui_clock();
}

void hint_paint(const char *pad, const char *kbd)
{
    scr_band(13, PAL_RED);
    scr_put(26, 1, pad, SCR_COLS - 1);
    scr_put(27, 1, inp_kbd ? kbd : "", SCR_COLS - 1);
}

/* The flat screens have no bar, and no clock: they are the ones up while a
   device call is running and the clock is stopped anyway. */
void header_off(void)
{
    clock_on = 0;
}

void ui_clock(void)
{
    char c[6];

    if (!clock_on)
        return;
    if (!gm_clock_ok) {
        scr_put(0, CLOCK_END - 5, "", 5);
        return;
    }

    c[0] = (char) ('0' + gm_h / 10);
    c[1] = (char) ('0' + gm_h % 10);
    c[2] = ':';
    c[3] = (char) ('0' + gm_mi / 10);
    c[4] = (char) ('0' + gm_mi % 10);
    c[5] = '\0';
    scr_put(0, CLOCK_END - 5, c, 5);
}
