/*
 * Screen painters.
 *
 * Two kinds of screen. The banded ones -- inbox and message reader -- run the
 * DLI chain and are laid out to its three regions: header rows 0-2, content
 * rows 3-22, footer row 23. The transient ones -- splash, busy, error -- are
 * flat, which is deliberate rather than lazy: they are exactly the screens
 * that are up while SIO is running, and SIO does not want interrupts stealing
 * its cycles.
 */

#include <stdlib.h>
#include <string.h>

#include <atari.h>

#include "../gmail.h"
#include "platform.h"

/* Inbox geometry. */
#define LIST_TOP        3                       /* rows 3..18, 16 entries */
#define DETAIL_ROW      20                      /* rows 20..21 */

#define COL_SEL         0
#define COL_MARK        1
#define COL_FROM        3
#define W_FROM          13
#define COL_SUBJ        17
#define W_SUBJ          23

/* Message reader geometry: body occupies rows 3..20. */
#define MSG_TOP         3

/* Splash logo: LOGO_LARGE is four text rows tall. */
#define SPLASH_LOGO_ROW 6
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_COLS) / 2)

static char sbuf[64];
static char detbuf[ENT_NAME_LEN + ENT_SUBJ_LEN + 4];
static char wrapped[2][SCR_COLS + 1];
static char datebuf[ENT_DATE_LEN];

/* ------------------------------------------------------------------ */
/* Transient screens                                                   */
/* ------------------------------------------------------------------ */

static void flat_screen(void)
{
    dli_flat(C_FLAT_BG, C_FLAT_FG);
    scr_clear();
    pmg_show(LOGO_LARGE, SPLASH_LOGO_ROW, SPLASH_LOGO_COL);
}

void ui_splash(void)
{
    flat_screen();
    scr_center(11, "FujiNet Gmail", 0);
    scr_center(13, "Waiting for FujiNet", 0);
}

void ui_notfound(void)
{
    flat_screen();
    scr_center(11, "FujiNet not found", 0);
    scr_center(12, "Check the adapter", 0);
    scr_center(15, "PRESS ANY KEY", 0);
}

void ui_busy(unsigned char reason)
{
    flat_screen();
    if (reason == BUSY_INDEX) {
        scr_center(11, "Opening mailbox...", 0);
        scr_center(13, "up to 60 seconds", 0);
    } else {
        scr_center(11, "Fetching message...", 0);
    }
}

void ui_error(unsigned char code)
{
    const char *l1;
    const char *l2 = 0;

    switch (code) {
    case GM_NOAUTH:
        l1 = "Authorize Google in";
        l2 = "the FujiNet Web UI";
        break;
    case GM_DENIED:
        l1 = "Google access denied";
        l2 = "re-authorize (scope)";
        break;
    case GM_NOTFOUND:
        l1 = "Message not found";
        break;
    case GM_NOSERVICE:
        l1 = "Service unavailable";
        l2 = "check connection";
        break;
    case 0:
        l1 = "No reply from";
        l2 = "FujiNet (timeout)";
        break;
    default:
        strcpy(sbuf, "Error ");
        utoa(code, sbuf + 6, 10);
        l1 = sbuf;
        break;
    }

    flat_screen();
    scr_center(11, "Gmail error", 0);
    scr_center(13, l1, 0);
    if (l2)
        scr_center(14, l2, 0);

    /* The raw codes underneath the friendly text. Worth the line: when
       something goes wrong on real hardware this is the difference between a
       reportable bug and "it just says error". */
    strcpy(sbuf, gm_stage);
    strcat(sbuf, " code ");
    utoa(code, sbuf + strlen(sbuf), 10);
    strcat(sbuf, " dev ");
    utoa(gm_dev_ecode, sbuf + strlen(sbuf), 10);
    scr_center(16, sbuf, 0);

    scr_center(18, "PRESS ANY KEY", 0);
}

/* ------------------------------------------------------------------ */
/* Inbox                                                               */
/* ------------------------------------------------------------------ */

static void page_indicator(void)
{
    char n[12];

    if (gm_count == 0) {
        sbuf[0] = '\0';
        return;
    }

    ultoa(gm_range + 1, sbuf, 10);
    strcat(sbuf, "-");
    ultoa(gm_range + gm_count, n, 10);
    strcat(sbuf, n);
    strcat(sbuf, "/");
    ultoa(gm_total, n, 10);
    strcat(sbuf, n);
}

static void draw_entry(unsigned char slot)
{
    struct entry *e   = &gm_index[slot];
    unsigned char row = LIST_TOP + slot;
    unsigned char inv = (slot == gm_sel);

    scr_field(row, COL_SEL,  inv ? ">" : " ", 1, inv);
    scr_field(row, COL_MARK, e->unread ? "*" : " ", 1, inv);
    scr_field(row, 2, "", 1, inv);
    scr_field(row, COL_FROM, e->name, W_FROM, inv);
    scr_field(row, COL_FROM + W_FROM, "", 1, inv);
    scr_field(row, COL_SUBJ, e->subject, W_SUBJ, inv);
}

/*
 * The list truncates both fields hard to fit 40 columns, so the two rows below
 * it carry the selected entry in full. It earns the space the Intellivision
 * version had to reclaim by bounce-scrolling the highlighted row.
 */
/*
 * The selected message's date, on the free half of the page-indicator row.
 *
 * Forty columns has no room for a date column in the list -- the two fields
 * already truncate hard -- but there is exactly one message whose date matters
 * at any moment, and row 2 was empty to the left of the indicator. It moves
 * with the selection, so it reads as belonging to the highlighted row.
 */
static void draw_date(void)
{
    if (gm_count == 0) {
        scr_field(2, 1, "", ENT_DATE_LEN, 0);
        return;
    }

    date_fmt(datebuf, gm_index[gm_sel].ts);
    scr_field(2, 1, datebuf, ENT_DATE_LEN, 0);
}

static void draw_detail(void)
{
    unsigned int n;
    unsigned char i;

    draw_date();

    if (gm_count == 0) {
        scr_row_clear(DETAIL_ROW);
        scr_row_clear(DETAIL_ROW + 1);
        return;
    }

    strcpy(detbuf, gm_index[gm_sel].name);
    strcat(detbuf, ": ");
    strcat(detbuf, gm_index[gm_sel].subject);

    n = wrap_text(detbuf, wrapped[0], 2, SCR_COLS, SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field(DETAIL_ROW + i, 0, (i < n) ? wrapped[i] : "", SCR_COLS, 0);
}

void ui_inbox(void)
{
    unsigned char i;

    dli_bands();
    scr_clear();
    pmg_show(LOGO_SMALL, 0, 1);

    scr_text(1, 13, "Gmail", 0);
    scr_text(1, 20, "Inbox", 0);

    page_indicator();
    scr_right(2, 38, sbuf, 0);

    if (gm_count == 0) {
        scr_center(10, "No messages", 0);
    } else {
        for (i = 0; i < gm_count; i++)
            draw_entry(i);
    }

    draw_detail();
    scr_text(FOOT_ROW, 1, "RET:READ  <>:PAGE  R:REFRESH  ESC:QUIT", 0);
}

/* Repaint only what a selection move touched. gm_sel is already the new one. */
void ui_inbox_sel(unsigned char from, unsigned char to)
{
    if (from < gm_count)
        draw_entry(from);
    if (to < gm_count)
        draw_entry(to);
    draw_detail();
}

/* ------------------------------------------------------------------ */
/* Message reader                                                      */
/* ------------------------------------------------------------------ */

void ui_message(unsigned int top)
{
    char n[12];
    unsigned int  pages;
    unsigned int  row;
    unsigned int  nsub;
    unsigned char i;

    dli_bands();
    pmg_hide();

    strcpy(sbuf, "From: ");
    strcat(sbuf, gm_index[gm_sel].name);
    scr_field(0, 0, sbuf, SCR_COLS, 0);

    nsub = wrap_text(gm_index[gm_sel].subject, wrapped[0], 2,
                     SCR_COLS, SCR_COLS + 1);
    scr_field(1, 0, wrapped[0], SCR_COLS, 0);
    scr_field(2, 0, (nsub > 1) ? wrapped[1] : "", SCR_COLS, 0);

    if (gm_body_rows == 0) {
        scr_field(MSG_TOP, 0, "(no text content)", SCR_COLS, 0);
        for (i = 1; i < MSG_ROWS; i++)
            scr_row_clear(MSG_TOP + i);
    } else {
        for (i = 0; i < MSG_ROWS; i++) {
            row = top + i;
            scr_field(MSG_TOP + i, 0,
                      (row < gm_body_rows) ? gm_body[row] : "", SCR_COLS, 0);
        }
    }

    scr_rows_clear(MSG_TOP + MSG_ROWS, FOOT_ROW - 1);

    scr_row_clear(FOOT_ROW);
    scr_text(FOOT_ROW, 1, "^v:LINE  <>:PAGE  ESC:BACK", 0);

    pages = (gm_body_rows + MSG_ROWS - 1) / MSG_ROWS;
    if (pages == 0)
        pages = 1;

    /* The last page is clamped flush against the end of the body, so its
       offset is not a whole multiple of a page. Report it as the last page
       anyway -- otherwise the bottom of a message reads "15/16" and looks
       like there is still somewhere to scroll. */
    if (gm_body_rows > MSG_ROWS && top >= gm_body_rows - MSG_ROWS)
        utoa(pages, sbuf, 10);
    else
        utoa(top / MSG_ROWS + 1, sbuf, 10);
    strcat(sbuf, "/");
    utoa(pages, n, 10);
    strcat(sbuf, n);
    if (gm_body_trunc)
        strcat(sbuf, "+");
    scr_right(FOOT_ROW, 38, sbuf, 0);
}
