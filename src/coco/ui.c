/*
 * Screen painters.
 *
 * Three screens: the inbox, the message reader, and the flat ones -- splash,
 * busy, error -- that share a layout and differ only in their text.
 *
 * Thirty-two columns by sixteen rows is the smallest screen this client has
 * had, two-thirds of the Atari's rows and four-tenths of the Apple's cells, and
 * every row here is spoken for. The header is two rows because the mark is two
 * rows. The panel under the list is two rows because a 12-column From and a
 * 17-column Subject truncate hard enough that the selected entry has to be
 * spelled out somewhere. What is left -- eleven rows -- is the page size, which
 * is why IDX_MAX comes down to 11 on this platform rather than the list
 * scrolling inside a page.
 *
 * Every string literal is uppercase: the 6847 has sixty-four glyphs and no
 * lowercase, so sc() would fold them anyway and writing them folded is the only
 * way the source says what the screen shows.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/* ------------------------------------------------------------------ */

/* List columns. Column 0 is the unread chip and is deliberately outside the
   selection bar -- see the rule in platform.h. */
#define COL_CHIP        0
#define COL_FROM        2
#define W_FROM          12
#define COL_SUBJ        15
#define W_SUBJ          (SCR_COLS - COL_SUBJ)   /* 17 */

/* Header row 1: the selected message's date on the left, the page indicator
   flush right, and one blank column between them that neither may cross. */
#define DATE_COL        HDR_TEXT_COL            /* 8, cols 8-19  */
#define W_DATE          (ENT_DATE_LEN - 1)      /* 12            */
#define PAGE_COL        (DATE_COL + W_DATE + 1) /* 21            */
#define W_PAGE          (SCR_COLS - PAGE_COL)   /* 11, cols 21-31 */

/* Reader: sender and date on row 0, subject wrapped over rows 1-2. */
#define MSG_NAME_W      19                      /* cols 0-18 */
#define MSG_DATE_COL    (SCR_COLS - W_DATE)     /* cols 20-31 */

/* Flat screens. logo_large() takes the top-left of the mark and draws its
   frame in the ring outside, so the frame lands on rows 2-8, cols 10-21. */
#define FLAT_LOGO_ROW   3
#define FLAT_LOGO_COL   ((SCR_COLS - LOGO_LARGE_COLS) / 2)
#define FLAT_HEAD       10
#define FLAT_BODY       11
#define FLAT_ANYKEY     14

static char sbuf[40];
static char nbuf[12];
static char detbuf[ENT_NAME_LEN + ENT_SUBJ_LEN + 4];
static char wrapped[2][SCR_COLS + 1];
static char datebuf[ENT_DATE_LEN];

/* ------------------------------------------------------------------ */
/* Flat screens                                                        */
/* ------------------------------------------------------------------ */

static void flat_screen(void)
{
    scr_clear();
    logo_large(FLAT_LOGO_ROW, FLAT_LOGO_COL);
}

void ui_splash(void)
{
    flat_screen();
    scr_center(FLAT_HEAD, "FUJINET GMAIL", 0);
    scr_center(FLAT_BODY + 1, "WAITING FOR FUJINET", 0);
}

void ui_notfound(void)
{
    flat_screen();
    scr_center(FLAT_HEAD, "FUJINET NOT FOUND", 0);
    scr_center(FLAT_BODY, "CHECK THE ADAPTER", 0);
    scr_center(FLAT_ANYKEY, "PRESS ANY KEY", 0);
}

void ui_busy(unsigned char reason)
{
    flat_screen();

    if (reason == BUSY_INDEX) {
        scr_center(FLAT_HEAD, "OPENING MAILBOX...", 0);
        scr_center(FLAT_BODY + 1, "UP TO 60 SECONDS", 0);
    } else {
        scr_center(FLAT_HEAD, "FETCHING MESSAGE...", 0);
    }
}

void ui_error(unsigned char code)
{
    const char *l1;
    const char *l2 = 0;

    switch (code) {
    case GM_NOAUTH:
        l1 = "AUTHORIZE GOOGLE IN";
        l2 = "THE FUJINET WEB UI";
        break;
    case GM_DENIED:
        l1 = "GOOGLE ACCESS DENIED";
        l2 = "RE-AUTHORIZE (SCOPE)";
        break;
    case GM_NOTFOUND:
        l1 = "MESSAGE NOT FOUND";
        break;
    case GM_NOSERVICE:
        l1 = "SERVICE UNAVAILABLE";
        l2 = "CHECK CONNECTION";
        break;
    case 0:
        l1 = "NO REPLY FROM";
        l2 = "FUJINET (TIMEOUT)";
        break;
    default:
        strcpy(sbuf, "ERROR ");
        utoa(code, sbuf + 6, 10);
        l1 = sbuf;
        break;
    }

    flat_screen();
    scr_center(FLAT_HEAD, "GMAIL ERROR", 0);
    scr_center(FLAT_BODY, l1, 0);
    if (l2)
        scr_center(FLAT_BODY + 1, l2, 0);

    /* The raw codes underneath the friendly text. Worth the line: when
       something goes wrong on real hardware this is the difference between a
       reportable bug and "it just says error". It has to be built after the
       default branch above has finished with sbuf. */
    strcpy(sbuf, gm_stage);
    strcat(sbuf, " CODE ");
    utoa(code, nbuf, 10);
    strcat(sbuf, nbuf);
    strcat(sbuf, " DEV ");
    utoa(gm_dev_ecode, nbuf, 10);
    strcat(sbuf, nbuf);
    scr_center(FLAT_BODY + 2, sbuf, 0);

    scr_center(FLAT_ANYKEY + 1, "PRESS ANY KEY", 0);
}

/* ------------------------------------------------------------------ */
/* Inbox                                                               */
/* ------------------------------------------------------------------ */

/*
 * "1-11/137" in the eleven columns to the right of the date.
 *
 * A mailbox big enough to outrun those eleven columns is not hypothetical --
 * six digits of total is 17 characters in the long form -- and there is a date
 * immediately to the left for it to run into. So the form steps down rather
 * than overflowing: the range, then the position alone, then the size alone.
 * Eleven columns holds the last of those for any mailbox that fits in the
 * unsigned long the wire sends.
 */
static void page_indicator(void)
{
    if (gm_count == 0) {
        sbuf[0] = '\0';
        return;
    }

    ultoa(gm_range + 1, sbuf, 10);
    strcat(sbuf, "-");
    ultoa(gm_range + gm_count, nbuf, 10);
    strcat(sbuf, nbuf);
    strcat(sbuf, "/");
    ultoa(gm_total, nbuf, 10);
    strcat(sbuf, nbuf);
    if (strlen(sbuf) <= W_PAGE)
        return;

    ultoa(gm_range + 1, sbuf, 10);
    strcat(sbuf, "/");
    ultoa(gm_total, nbuf, 10);
    strcat(sbuf, nbuf);
    if (strlen(sbuf) <= W_PAGE)
        return;

    ultoa(gm_total, sbuf, 10);
}

/*
 * The selected message's date, on the left of header row 1.
 *
 * There is no date column in the list -- From and Subject already truncate hard
 * at twelve and seventeen -- but there is exactly one message whose date
 * matters at any moment. It moves with the selection, so it reads as belonging
 * to the highlighted row. The Atari does the same thing for the same reason.
 */
static void draw_date(void)
{
    if (gm_count == 0) {
        scr_field(1, DATE_COL, "", W_DATE, 0);
        return;
    }

    date_fmt(datebuf, gm_index[gm_sel].ts);
    scr_field(1, DATE_COL, datebuf, W_DATE, 0);
}

/*
 * One list row.
 *
 * Column 0 is the unread chip and is written raw: solid Gmail red for unread,
 * black for read. It is never inverted, because XOR $40 on a semigraphics byte
 * is part of the colour field -- an inverted red chip would come out cyan
 * rather than highlighted. The selection bar therefore starts at column 1,
 * which also gives the chip a black or red edge to read against on every row.
 */
static void draw_entry(unsigned char slot)
{
    struct entry *e   = &gm_index[slot];
    unsigned char row = (unsigned char) (LIST_TOP + slot);
    unsigned char inv = (unsigned char) (slot == gm_sel);

    scr_cell(row, COL_CHIP, e->unread ? GM_RED : SG_BLACK);
    scr_field(row, 1, "", 1, inv);
    scr_field(row, COL_FROM, e->name, W_FROM, inv);
    scr_field(row, (unsigned char) (COL_FROM + W_FROM), "", 1, inv);
    scr_field(row, COL_SUBJ, e->subject, W_SUBJ, inv);
}

/*
 * The two rows under the list, which spell the selection out in full.
 *
 * They earn their keep harder here than on the Atari, whose list column is
 * thirty rather than seventeen. Sixty-four cells is more of a name and subject
 * than either fixed field holds, and wrap_text() breaks it on a word.
 */
static void draw_panel(void)
{
    unsigned int  n;
    unsigned char i;

    if (gm_count == 0) {
        scr_rows_clear(PANEL_ROW, (unsigned char) (PANEL_ROW + 1));
        return;
    }

    strcpy(detbuf, gm_index[gm_sel].name);
    strcat(detbuf, ": ");
    strcat(detbuf, gm_index[gm_sel].subject);

    n = wrap_text(detbuf, (char *) wrapped, 2, SCR_COLS, SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (PANEL_ROW + i), 0,
                  (i < n) ? (const char *) wrapped[i] : "", SCR_COLS, 0);
}

void ui_inbox(void)
{
    unsigned char i;

    scr_clear();
    logo_small(LOGO_ROW, LOGO_COL);

    scr_text(0, HDR_TEXT_COL, "GMAIL", 0);
    scr_right(0, RIGHT_COL, "INBOX", 0);

    draw_date();
    page_indicator();
    scr_right(1, RIGHT_COL, sbuf, 0);

    if (gm_count == 0) {
        scr_center((unsigned char) (LIST_TOP + 5), "NO MESSAGES", 0);
    } else {
        for (i = 0; i < gm_count; i++)
            draw_entry(i);
    }

    draw_panel();
    scr_field(FOOT_ROW, 0, "ENT:READ <>:PAGE R:REFR Q:QUIT", SCR_COLS, 0);
}

/* Repaint only what a selection move touched. gm_sel is already the new one,
   and the page indicator is not a function of it. */
void ui_inbox_sel(unsigned char from, unsigned char to)
{
    if (from < gm_count)
        draw_entry(from);
    if (to < gm_count)
        draw_entry(to);

    draw_date();
    draw_panel();
}

/* ------------------------------------------------------------------ */
/* Message reader                                                      */
/* ------------------------------------------------------------------ */

/*
 * Row 0 is the sender and the date, with no "FROM:" label.
 *
 * The Atari and the Apple both spell the label out; six of thirty-two columns
 * is a fifth of the row to say something the top line of a mail reader always
 * means. Spending them on the date instead puts both of the things you want
 * before reading on the one row there is for them.
 */
void ui_message(unsigned int top)
{
    unsigned int  pages;
    unsigned int  row;
    unsigned int  nsub;
    unsigned char i;

    scr_field(0, 0, gm_index[gm_sel].name, MSG_NAME_W, 0);
    scr_field(0, (unsigned char) (MSG_NAME_W), "", 1, 0);
    date_fmt(datebuf, gm_index[gm_sel].ts);
    scr_field(0, MSG_DATE_COL, datebuf, W_DATE, 0);

    nsub = wrap_text(gm_index[gm_sel].subject, (char *) wrapped, 2,
                     SCR_COLS, SCR_COLS + 1);
    scr_field(1, 0, wrapped[0], SCR_COLS, 0);
    scr_field(2, 0, (nsub > 1) ? (const char *) wrapped[1] : "", SCR_COLS, 0);

    if (gm_body_rows == 0) {
        scr_field(MSG_TOP, 0, "(NO TEXT CONTENT)", SCR_COLS, 0);
        for (i = 1; i < MSG_ROWS; i++)
            scr_row_clear((unsigned char) (MSG_TOP + i));
    } else {
        for (i = 0; i < MSG_ROWS; i++) {
            row = top + i;
            scr_field((unsigned char) (MSG_TOP + i), 0,
                      (row < gm_body_rows) ? (const char *) gm_body[row] : "",
                      SCR_COLS, 0);
        }
    }

    /* No gap to clear between the body and the footer: MSG_TOP + MSG_ROWS is
       FOOT_ROW exactly. The Atari has two spare rows there and clears them;
       doing the same here would ask scr_rows_clear() for a negative run. */

    scr_field(FOOT_ROW, 0, "^V:LINE <>:PAGE BRK:BACK", SCR_COLS, 0);

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
        utoa((unsigned int) (top / MSG_ROWS + 1), sbuf, 10);
    strcat(sbuf, "/");
    utoa(pages, nbuf, 10);
    strcat(sbuf, nbuf);
    if (gm_body_trunc)
        strcat(sbuf, "+");
    scr_right(FOOT_ROW, RIGHT_COL, sbuf, 0);
}
