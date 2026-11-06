/*
 * Screen painters.
 *
 * Two kinds of screen, the split the Apple backend makes and the Google
 * Calendar client's H89 port keeps. The banded ones -- inbox, reader and
 * form -- have the app bar across row 0 with the mark and a title, a status
 * row with the clock, and the keys on the 25th line. The flat ones -- splash,
 * not found, error, and the busy screen at boot -- are the large mark and
 * some centred text.
 *
 * What the H19 changes is the busy screen. A banded screen that has to wait
 * on the FujiNet says so on its own status row and on the 25th line and
 * stays where it is: blanking it for a busy screen and painting it all back
 * afterwards would be two full screens, four seconds at 9600 baud, around
 * every fetch. The progress bar goes on the 25th line the same way, and
 * since only the cells that change are sent, a step of it is a few bytes.
 *
 * And the reader scrolls. A line up or down is the H19's own delete and
 * insert line round the body (h19_scroll), so what goes down the wire is the
 * one line that came in rather than nineteen that moved.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"
#include "ui_int.h"

char          ui_sbuf[96];
char          ui_wrapped[2][SCR_COLS + 1];
char          ui_datebuf[ENT_DATE_LEN];
unsigned char ui_banded;

unsigned char ui_screen;

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

void ui_num(char *dst, unsigned int v)
{
    utoa(v, dst, 10);
}

/* Two digits, zero padded, straight into dst[0..1]. */
static void num2(char *dst, unsigned char v)
{
    dst[0] = (char) ('0' + (v / 10) % 10);
    dst[1] = (char) ('0' + v % 10);
}

/* "3 of 16" style counts into ui_sbuf, after whatever is there. */
void ui_cat_num(unsigned int v)
{
    ui_num(ui_sbuf + strlen(ui_sbuf), v);
}

/* ------------------------------------------------------------------ */
/* Banded chrome                                                       */
/* ------------------------------------------------------------------ */

void ui_band(const char *title)
{
    scr_clear();
    scr_fill(0, 0, ' ' | REV, SCR_COLS);        /* the app bar */
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, title, 1);
    ui_banded = 1;
}

/* The wall clock, five cells right-aligned on row 2 of a banded screen. */
void ui_clock(void)
{
    char t[6];

    if (!ui_banded)
        return;

    if (!gm_clock_ok) {
        scr_field(2, CLOCK_COL, "", 5, 0);
        return;
    }

    num2(t, gm_h);
    t[2] = ':';
    num2(t + 3, gm_mi);
    t[5] = '\0';
    scr_field(2, CLOCK_COL, t, 5, 0);
}

/* Row 2, left of the clock: where a banded screen says what it is doing. */
void ui_status(const char *s, unsigned char inv)
{
    scr_field(2, HDR_TEXT_COL, s, STATUS_W, inv);
}

/* ------------------------------------------------------------------ */
/* Flat screens                                                        */
/* ------------------------------------------------------------------ */

void ui_flat(void)
{
    scr_clear();
    scr_keys(0);
    logo_large(SPLASH_LOGO_ROW, SPLASH_LOGO_COL);
    ui_banded = 0;
    ui_screen = UI_FLAT;
}

/* A flat screen is up just before a bus call that may take a while, so it
   goes to the terminal now rather than at the next key read. */
void ui_splash(void)
{
    ui_flat();
    scr_center(FLAT_TEXT_ROW, "FujiNet Gmail", 0);
    scr_center(FLAT_TEXT_ROW + 2, "Looking for FujiNet", 0);
    h19_flush();
}

void ui_notfound(void)
{
    ui_flat();
    scr_center(FLAT_TEXT_ROW, "FujiNet not found", 0);
    scr_center(FLAT_TEXT_ROW + 2,
               "Check the card and that the FujiNet is running", 0);
    scr_center(FLAT_TEXT_ROW + 5, "Press any key to try again", 0);
}

void ui_busy(unsigned char reason)
{
    const char *s;

    switch (reason) {
    case BUSY_INDEX:    s = "Opening mailbox...";   break;
    case BUSY_SEND:     s = "Sending message...";   break;
    default:            s = "Fetching message...";  break;
    }

    if (ui_banded) {
        h19_cursor(H19_NONE, 0);        /* the form's, while it sends */
        ui_status(s, 1);
        scr_keys((reason == BUSY_INDEX)
                 ? "Waiting for the FujiNet -- up to a minute for the inbox"
                 : "Waiting for the FujiNet");
    } else {
        ui_flat();
        scr_center(FLAT_TEXT_ROW, s, 0);
        if (reason == BUSY_INDEX)
            scr_center(FLAT_TEXT_ROW + 2, "up to a minute", 0);
    }

    h19_flush();
}

void ui_error(unsigned char code)
{
    const char *l1;
    const char *l2 = 0;
    char n[8];

    switch (code) {
    case GM_NOAUTH:
        l1 = "Authorize Google in the FujiNet Web UI";
        break;
    case GM_DENIED:
        l1 = "Google access denied";
        l2 = "re-authorize in the Web UI -- the grant is missing a scope";
        break;
    case GM_NOTFOUND:
        l1 = "Message not found";
        l2 = "refresh the inbox";
        break;
    case GM_REJECTED:
        l1 = "Draft rejected";
        l2 = "check the address";
        break;
    case GM_TOOBIG:
        l1 = "Message too large";
        break;
    case GM_NOSERVICE:
        l1 = "Service unavailable";
        l2 = "check the connection";
        break;
    case 0:
        /* 0 is no reply at all: the calls failed, so there is no device
           status to name. */
        l1 = "No reply from the FujiNet";
        break;
    default:
        strcpy(ui_sbuf, "Error ");
        ui_cat_num(code);
        l1 = ui_sbuf;
        break;
    }

    ui_flat();
    scr_center(FLAT_TEXT_ROW - 1, "Gmail error", 0);
    scr_center(FLAT_TEXT_ROW + 1, l1, 0);
    if (l2)
        scr_center(FLAT_TEXT_ROW + 2, l2, 0);

    /* The raw codes underneath, because "Gmail error" on its own is not
       something anyone can act on. ui_sbuf may be l1, so they go in a buffer
       of their own. */
    {
        static char raw[40];

        strcpy(raw, gm_stage);
        strcat(raw, " code ");
        ui_num(n, code);
        strcat(raw, n);
        strcat(raw, " dev ");
        ui_num(n, gm_dev_ecode);
        strcat(raw, n);
        scr_center(FLAT_TEXT_ROW + 4, raw, 0);
    }

    scr_center(FLAT_TEXT_ROW + 7, "Press any key", 0);
}
