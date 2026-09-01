/*
 * Screen painters.
 *
 * Two kinds of screen, the same split the Atari backend makes. The banded ones
 * -- inbox and message reader -- run the three-region layout: header rows 0-2,
 * content rows 3-22, footer row 23. The transient ones -- splash, busy, error
 * -- are flat.
 *
 * There the three bands are three colours poked into a display list, and the
 * flat screens are flat because they are the ones up while SIO is running and
 * SIO does not want interrupts stealing its cycles. Here the bands are inverse
 * video -- row 0 is a solid white app bar with black text on it, which is what
 * an app bar looks like on a machine with one bit per pixel -- and the flat
 * screens are flat only because there is no chrome worth putting on them.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"

/* Header: the mark at cols 1-6 rows 0-2, everything else to the right of it. */
#define LOGO_ROW        0
#define LOGO_COL        1
#define HDR_TEXT_COL    8

/*
 * Inbox geometry: rows 3-18 are the sixteen entries, row 19 a rule, rows 20-22
 * the selection spelled out.
 *
 * Column 0 is the unread marker and the selection bar starts at column 1. The
 * Atari puts the marker inside its bar because a 40-column row has no column to
 * spare; here it has to stay outside for a reason of its own. MouseText occupies
 * the very character codes the inverse forms would have used, so there is no
 * inverse of a glyph -- a diamond inside the bar would fall back to ASCII and
 * change shape on exactly the row the cursor is on.
 */
#define LIST_TOP        CONTENT_TOP             /* 3 */
#define RULE_ROW        19
#define DETAIL_ROW      20                      /* rows 20..22 */

#define COL_MARK        0
#define COL_SEL         1
#define COL_DATE        3
#define W_DATE          13
#define COL_FROM        17
#define W_FROM          20
#define COL_SUBJ        38
#define W_SUBJ          (RIGHT_COL + 1 - COL_SUBJ)      /* 41 */

/* Message reader geometry: body occupies rows 3..22. */
#define MSG_TOP         CONTENT_TOP

/* Splash: the large mark, centred. */
#define SPLASH_LOGO_ROW 5
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_LARGE_COLS) / 2)

static char sbuf[128];
static char detbuf[ENT_NAME_LEN + ENT_SUBJ_LEN + 4];
static char wrapped[2][SCR_COLS + 1];
static char datebuf[ENT_DATE_LEN];

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void num(char *dst, unsigned int v)
{
    utoa(v, dst, 10);
}

/*
 * Row 23, in normal video.
 *
 * The app bar above is inverse and this one deliberately is not, because this
 * is the row with the arrows on it. MouseText lives at the character codes the
 * inverse forms would otherwise occupy, so there is no inverse of a glyph -- an
 * inverse hint bar would have to fall back to "^v<>" and lose the one place the
 * glyphs are worth the most. Drawing hints on ordinary background is also what
 * every Apple II program that uses MouseText does.
 */
static void footer(const char *hints, const char *right)
{
    scr_row_clear(FOOT_ROW);
    if (hints)
        scr_text(FOOT_ROW, 1, hints, 0);
    if (right && *right)
        scr_right(FOOT_ROW, RIGHT_COL, right, 0);
}

/* ------------------------------------------------------------------ */
/* Transient screens                                                   */
/* ------------------------------------------------------------------ */

static void flat_screen(void)
{
    scr_clear();
    logo_large(SPLASH_LOGO_ROW, SPLASH_LOGO_COL);
}

void ui_splash(void)
{
    flat_screen();
    scr_center(13, "FujiNet Gmail", 0);
    scr_center(15, "Waiting for FujiNet", 0);
}

void ui_notfound(void)
{
    flat_screen();
    scr_center(13, "FujiNet not found", 0);
    scr_center(14, "Check the adapter", 0);
    scr_center(17, "PRESS ANY KEY", 0);
}

void ui_busy(unsigned char reason)
{
    flat_screen();
    if (reason == BUSY_INDEX) {
        scr_center(13, MT_HOURGLASS " Opening mailbox...", 0);
        scr_center(15, "up to 60 seconds", 0);
    } else if (reason == BUSY_SEND) {
        scr_center(13, MT_HOURGLASS " Sending message...", 0);
    } else {
        scr_center(13, MT_HOURGLASS " Fetching message...", 0);
    }
}

void ui_sent(void)
{
    flat_screen();
    scr_center(13, "Message sent", 0);
    scr_center(17, "PRESS ANY KEY", 0);
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
        /*
         * Not "timeout" here. SmartPort has no host-side timeout to expire --
         * the call is into card firmware that blocks until the FujiNet answers
         * -- so code 0 on this bus means the device replied and the reply told
         * us nothing, which is a different thing to report.
         */
        l1 = "No usable reply from FujiNet";
        break;
    default:
        strcpy(sbuf, "Error ");
        num(sbuf + 6, code);
        l1 = sbuf;
        break;
    }

    flat_screen();
    scr_center(13, "Gmail error", 0);
    scr_center(15, l1, 0);
    if (l2)
        scr_center(16, l2, 0);

    /* The raw codes underneath the friendly text. Worth the line: when
       something goes wrong on real hardware this is the difference between a
       reportable bug and "it just says error". */
    strcpy(sbuf, gm_stage);
    strcat(sbuf, " code ");
    num(n, code);
    strcat(sbuf, n);
    strcat(sbuf, " dev ");
    num(n, gm_dev_ecode);
    strcat(sbuf, n);
    scr_center(18, sbuf, 0);

    scr_center(20, "PRESS ANY KEY", 0);
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

    /* Outside the bar, and always in normal video -- see the header comment. */
    scr_field(row, COL_MARK, e->unread ? UNREAD_GLYPH : " ", 1, 0);

    scr_field(row, COL_SEL, inv ? ">" : " ", 1, inv);
    scr_field(row, COL_SEL + 1, "", 1, inv);

    date_fmt(datebuf, e->ts);
    scr_field(row, COL_DATE, datebuf, W_DATE, inv);
    scr_field(row, COL_FROM, e->name, W_FROM, inv);
    scr_field(row, COL_SUBJ, e->subject, W_SUBJ, inv);

    /* The single columns between the fields. They have to be painted with the
       row's own video or the selection bar comes out in segments with two
       holes punched through it -- the fields are not contiguous, and a cell
       nobody writes keeps whatever the last screen left there. Separate rather
       than folded into the field widths above so that a name of exactly W_FROM
       characters still cannot touch the subject. */
    scr_field(row, COL_DATE + W_DATE, "", COL_FROM - COL_DATE - W_DATE, inv);
    scr_field(row, COL_FROM + W_FROM, "", COL_SUBJ - COL_FROM - W_FROM, inv);

    /* Column 79 stays clear, so the bar stops one short of the edge. */
    scr_field(row, RIGHT_COL + 1, "", 1, 0);
}

/*
 * The two columns still truncate -- a subject is 128 characters on the wire and
 * 41 on the row -- so the band under the rule carries the selection in full.
 * On the Atari this compensates for a hard truncation of both fields at 40
 * columns; here it is only the subject that ever needs it, and there is room to
 * put the address on its own line underneath.
 */
static void draw_detail(void)
{
    unsigned int n;
    unsigned char i;

    if (gm_count == 0) {
        scr_rows_clear(DETAIL_ROW, DETAIL_ROW + 2);
        return;
    }

    strcpy(detbuf, gm_index[gm_sel].subject);
    n = wrap_text(detbuf, wrapped[0], 2, SCR_COLS - 2, SCR_COLS + 1);

    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (DETAIL_ROW + i), 2,
                  (i < n) ? wrapped[i] : "", SCR_COLS - 2, 0);

    strcpy(sbuf, "from ");
    strcat(sbuf, gm_index[gm_sel].name);
    scr_field(DETAIL_ROW + 2, 2, sbuf, SCR_COLS - 2, 0);
}

void ui_inbox(void)
{
    unsigned char i;

    scr_clear();

    scr_field(0, 0, "", SCR_COLS, 1);           /* the app bar */
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, "Gmail", 1);
    scr_text(0, HDR_TEXT_COL + 8, "Inbox", 1);

    page_indicator();
    scr_right(2, RIGHT_COL, sbuf, 0);

    if (gm_count == 0) {
        scr_center(10, "No messages", 0);
    } else {
        for (i = 0; i < gm_count; i++)
            draw_entry(i);
    }

    scr_fill(RULE_ROW, 2, MT_RULE, RIGHT_COL - 1, 0);
    draw_detail();

    footer("RET" MT_RETURN ":READ   " MT_UP MT_DOWN ":MOVE   "
           MT_LEFT MT_RIGHT ":PAGE   C:COMPOSE   R:REFRESH   ESC:QUIT", 0);
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

    scr_field(0, 0, "", SCR_COLS, 1);
    strcpy(sbuf, "From: ");
    strcat(sbuf, gm_index[gm_sel].name);
    scr_text(0, 1, sbuf, 1);

    date_fmt(datebuf, gm_index[gm_sel].ts);
    scr_right(0, RIGHT_COL, datebuf, 1);

    nsub = wrap_text(gm_index[gm_sel].subject, wrapped[0], 2,
                     SCR_COLS - 2, SCR_COLS + 1);
    scr_field(1, 1, wrapped[0], SCR_COLS - 2, 0);
    scr_field(2, 1, (nsub > 1) ? wrapped[1] : "", SCR_COLS - 2, 0);

    if (gm_body_rows == 0) {
        scr_field(MSG_TOP, 1, "(no text content)", SCR_COLS - 2, 0);
        for (i = 1; i < MSG_ROWS; i++)
            scr_row_clear((unsigned char) (MSG_TOP + i));
    } else {
        for (i = 0; i < MSG_ROWS; i++) {
            row = top + i;
            scr_field((unsigned char) (MSG_TOP + i), 1,
                      (row < gm_body_rows) ? gm_body[row] : "",
                      SCR_COLS - 2, 0);
        }
    }

    pages = (gm_body_rows + MSG_ROWS - 1) / MSG_ROWS;
    if (pages == 0)
        pages = 1;

    /* The last page is clamped flush against the end of the body, so its offset
       is not a whole multiple of a page. Report it as the last page anyway --
       otherwise the bottom of a message reads "15/16" and looks like there is
       still somewhere to scroll. */
    if (gm_body_rows > MSG_ROWS && top >= gm_body_rows - MSG_ROWS)
        num(sbuf, pages);
    else
        num(sbuf, (unsigned int) (top / MSG_ROWS + 1));
    strcat(sbuf, "/");
    num(n, pages);
    strcat(sbuf, n);
    if (gm_body_trunc)
        strcat(sbuf, "+");

    footer("R:REPLY   F:FORWARD   " MT_UP MT_DOWN ":LINE   "
           MT_LEFT MT_RIGHT ":PAGE   ESC:BACK", sbuf);
}

/* ------------------------------------------------------------------ */
/* Compose form                                                        */
/* ------------------------------------------------------------------ */

/*
 * The flat arrangement with eighty columns to spend: full-word labels, a
 * rule under the headers the way the inbox rules off its detail band, and
 * the widest field windows of the five backends -- the headers never even
 * scroll. The active field is a full inverse bar -- the list's selection
 * language -- with the cursor cell knocked back to normal video, a hole in
 * the bar. compose.c owns the cursor and the horizontal scroll; this end
 * paints what it is given.
 */

#define FRM_BODY_TOP    6
#define FRM_HINT_ROW    (FRM_BODY_TOP + FRM_NBODY + 1)
#define FRM_MSG_ROW     (FRM_HINT_ROW + 1)
#define FRM_VAL_COL     10      /* TO/SUBJECT value column */

#if FRM_MSG_ROW > 22
#error "the form no longer fits above the footer -- lower FRM_NBODY"
#endif

static unsigned char frm_row(unsigned char f)
{
    if (f == F_TO)
        return 3;
    if (f == F_SUBJ)
        return 4;
    return (unsigned char) (FRM_BODY_TOP + (f - F_BODY0));
}

static unsigned char frm_col(unsigned char f)
{
    return (unsigned char) ((f >= F_BODY0) ? 1 : FRM_VAL_COL);
}

/* One cell more than the storage width, so the cursor can sit past a full
   value; both header fields fit their windows whole. */
unsigned char ui_form_width(unsigned char f)
{
    return (unsigned char) ((f >= F_BODY0) ? (FRM_BODY_COLS + 1)
                                           : (FRM_TO_MAX + 1));
}

void ui_form(unsigned char mode)
{
    scr_clear();

    scr_field(0, 0, "", SCR_COLS, 1);           /* the app bar */
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, (mode == FRM_REPLY) ? "Reply"
                            : (mode == FRM_FWD)   ? "Forward"
                                                  : "New message", 1);

    scr_text(3, 2, "To", 0);
    scr_text(4, 2, "Subject", 0);
    scr_fill(5, 2, MT_RULE, RIGHT_COL - 1, 0);

    if (mode == FRM_REPLY)
        scr_text(FRM_HINT_ROW, 2,
                 "Blank To and Subject take the reply defaults", 0);

    footer("RET" MT_RETURN ":NEXT   " MT_UP MT_DOWN ":FIELD   "
           MT_LEFT MT_RIGHT ":CURSOR   DEL:ERASE   ESC:DONE", 0);
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    unsigned char w = ui_form_width(f);
    unsigned char col = frm_col(f);
    char          b[2];

    scr_field(frm_row(f), col, win, w, active);

    if (active) {
        b[0] = curx < strlen(win) ? win[curx] : ' ';
        b[1] = '\0';
        scr_field(frm_row(f), (unsigned char) (col + curx), b, 1, 0);
    }
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    scr_row_clear(FRM_MSG_ROW);

    switch (msg) {
    case FM_ASK:      s = "Send message? (Y/N)";     break;
    case FM_NEEDTO:   s = "A recipient is required"; break;
    case FM_NEEDBODY: s = "A message is required";   break;
    default:          return;                   /* FM_NONE: cleared above */
    }

    scr_center(FRM_MSG_ROW, s, 1);
}
