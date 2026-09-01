/*
 * Screen painters.
 *
 * Two kinds of screen, the same split every backend makes. The banded ones --
 * inbox and message reader -- run the three-region layout: header rows 0-2,
 * content rows 3-23, footer row 24, one row more of content than the Apple
 * because a PC text screen has 25. The transient ones -- splash, busy, error
 * -- are flat.
 *
 * One set of painters serves both widths. The geometry that differs is set
 * once in ui_geom() when screen.c has probed the mode; the few places the
 * *composition* differs -- where the date lives, how the selection is spelled
 * out -- branch on scr_wide. At 80 columns the layout is the Apple II's; at
 * 40 it is the Atari's, both taken from the mockups in README.md, so the
 * same machine shows one client or the other depending on how it booted.
 *
 * The attribute roles do what the Atari does with colour bands and the Apple
 * with inverse video, with one improvement neither could afford: unread
 * emphasis and the selection bar are independent attributes here, so an
 * unread row keeps its mark while selected instead of surrendering it to the
 * bar.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"

/* Inbox geometry: rows 3-18 are the sixteen entries, row 19 a rule, rows
   20-22 the selection spelled out, 23 blank. */
#define LIST_TOP        CONTENT_TOP             /* 3 */
#define RULE_ROW        19
#define DETAIL_ROW      20                      /* rows 20..22 */

/* Message reader geometry: body occupies rows 3..23. */
#define MSG_TOP         CONTENT_TOP

/* Splash: the large mark, centred. */
#define SPLASH_LOGO_ROW 5

/* Set by ui_geom() once screen.c knows the width. */
static unsigned char right_col;                 /* last text column       */

/*
 * Where the wall clock goes on the screen that is up, or CLK_NONE.
 *
 * The app bar's right-hand end on all three screens, except a wide reader,
 * where that corner already carries the message's own date and the clock
 * stands to the left of it. At forty columns the reader has no date on the bar
 * at all -- "From: " and a 32-character name fill it -- so the clock takes the
 * corner there too, five columns being cheaper than thirteen.
 *
 * The flat screens clear it: they are the ones up while the driver is talking
 * to the adapter, which is both the time nothing is pumping the clock and the
 * time a repaint is least welcome.
 */
#define CLK_NONE    0xFF
static unsigned char clk_col = CLK_NONE;
static char          clkbuf[6];
static unsigned char col_date, w_date;          /* 80 columns only        */
static unsigned char col_from, w_from;
static unsigned char col_subj, w_subj;

static char sbuf[128];
static char detbuf[ENT_NAME_LEN + ENT_SUBJ_LEN + 4];
static char wrapped[3][81];
static char datebuf[ENT_DATE_LEN];

void ui_geom(void)
{
    right_col = (unsigned char) (scr_cols - 2);

    if (scr_wide) {
        col_date = 3;  w_date = 13;
        col_from = 17; w_from = 20;
        col_subj = 38; w_subj = (unsigned char) (right_col + 1 - 38);
    } else {
        col_date = 0;  w_date = 0;              /* row 2 carries the date */
        col_from = 3;  w_from = 13;
        col_subj = 17; w_subj = 23;
    }
}

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void num(char *dst, unsigned int v)
{
    utoa(v, dst, 10);
}

static void footer(const char *hints, const char *right)
{
    scr_field(FOOT_ROW, 0, "", scr_cols, A_FOOT);
    if (hints)
        scr_text(FOOT_ROW, 1, hints, A_FOOT);
    if (right && *right)
        scr_right(FOOT_ROW, right_col, right, A_FOOT);
}

/* ------------------------------------------------------------------ */
/* Transient screens                                                   */
/* ------------------------------------------------------------------ */

/* HH:MM, right-aligned at clk_col in the app bar. Hand-rolled rather than
   utoa'd because a leading zero is not optional in a clock. */
void ui_clock(void)
{
    if (clk_col == CLK_NONE || !gm_clock_ok)
        return;

    clkbuf[0] = (char) ('0' + gm_h / 10);
    clkbuf[1] = (char) ('0' + gm_h % 10);
    clkbuf[2] = ':';
    clkbuf[3] = (char) ('0' + gm_mi / 10);
    clkbuf[4] = (char) ('0' + gm_mi % 10);
    clkbuf[5] = '\0';

    scr_right(0, clk_col, clkbuf, A_BAR);
}

static void flat_screen(void)
{
    clk_col = CLK_NONE;
    scr_clear();
    logo_large(SPLASH_LOGO_ROW,
               (unsigned char) ((scr_cols - LOGO_LARGE_COLS) / 2));
}

void ui_splash(void)
{
    flat_screen();
    scr_center(13, "FujiNet Gmail", A_TEXT);
    scr_center(15, "Waiting for FujiNet", A_TEXT);
}

/*
 * On this platform "not found" has one overwhelmingly likely cause with an
 * actionable fix, so the screen names it: fuji_msdos.c probes the INT F5
 * vector before anything touches the bus, and a null vector means no
 * FUJINET.SYS. A loaded driver with a dead adapter lands here too, which is
 * what the first line still covers.
 */
void ui_notfound(void)
{
    flat_screen();
    scr_center(13, "FujiNet not found", A_TEXT);
    scr_center(14, "Is FUJINET.SYS loaded in CONFIG.SYS?", A_TEXT);
    scr_center(17, "PRESS ANY KEY", A_TEXT);
}

void ui_busy(unsigned char reason)
{
    flat_screen();
    if (reason == BUSY_INDEX) {
        scr_center(13, "Opening mailbox...", A_TEXT);
        scr_center(15, "up to 60 seconds", A_TEXT);
    } else if (reason == BUSY_SEND) {
        scr_center(13, "Sending message...", A_TEXT);
    } else {
        scr_center(13, "Fetching message...", A_TEXT);
    }
}

void ui_sent(void)
{
    flat_screen();
    scr_center(13, "Message sent", A_TEXT);
    scr_center(17, "PRESS ANY KEY", A_TEXT);
}

void ui_error(unsigned char code)
{
    const char *l1;
    const char *l2 = 0;
    char n[8];

    switch (code) {
    case GM_NOAUTH:
        if (scr_wide) {
            l1 = "Authorize Google in the FujiNet Web UI";
        } else {
            l1 = "Authorize Google in";
            l2 = "the FujiNet Web UI";
        }
        break;
    case GM_DENIED:
        l1 = "Google access denied";
        l2 = scr_wide ? "re-authorize in the Web UI -- the grant is missing a scope"
                      : "re-authorize (scope)";
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
        l1 = "No reply from FujiNet";
        break;
    default:
        strcpy(sbuf, "Error ");
        num(sbuf + 6, code);
        l1 = sbuf;
        break;
    }

    flat_screen();
    scr_center(13, "Gmail error", A_TEXT);
    scr_center(15, l1, A_TEXT);
    if (l2)
        scr_center(16, l2, A_TEXT);

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
    scr_center(18, sbuf, A_TEXT);

    scr_center(20, "PRESS ANY KEY", A_TEXT);
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
    unsigned char row = (unsigned char) (LIST_TOP + slot);
    unsigned char sel = (slot == gm_sel);
    unsigned char a   = sel ? A_SEL : (e->unread ? A_EMPH : A_TEXT);

    if (scr_wide) {
        /* The mark keeps column 0 to itself, outside the bar, in its own
           emphasis -- the Apple's arrangement, minus the Apple's reason. */
        scr_field(row, 0, e->unread ? GL_UNREAD : " ", 1,
                  e->unread ? A_EMPH : A_TEXT);
        scr_field(row, 1, sel ? ">" : " ", 1, a);
        scr_field(row, 2, "", 1, a);

        date_fmt(datebuf, e->ts);
        scr_field(row, col_date, datebuf, w_date, a);
        scr_field(row, (unsigned char) (col_date + w_date), "",
                  (unsigned char) (col_from - col_date - w_date), a);
        scr_field(row, col_from, e->name, w_from, a);
        scr_field(row, (unsigned char) (col_from + w_from), "",
                  (unsigned char) (col_subj - col_from - w_from), a);
        scr_field(row, col_subj, e->subject, w_subj, a);

        /* Column 79 stays clear, so the bar stops one short of the edge. */
        scr_field(row, (unsigned char) (right_col + 1), "", 1, A_TEXT);
    } else {
        /* Forty columns has none to spare, so the mark rides inside the bar
           -- the Atari's row, with the diamond the Atari's character set did
           not have. */
        scr_field(row, 0, sel ? ">" : " ", 1, a);
        scr_field(row, 1, e->unread ? GL_UNREAD : " ", 1, a);
        scr_field(row, 2, "", 1, a);
        scr_field(row, col_from, e->name, w_from, a);
        scr_field(row, (unsigned char) (col_from + w_from), "", 1, a);
        scr_field(row, col_subj, e->subject, w_subj, a);
    }
}

/*
 * The list truncates its fields to fit the row, so the band under the rule
 * carries the selection in full. At 80 columns that is the Apple's shape --
 * the subject on two lines, the sender under it. At 40 it is the Atari's
 * "name: subject" run-in, given three rows where the Atari's 24-line screen
 * could only spare two, plus the Atari's trick of putting the selected
 * message's date on the free half of the indicator row.
 */
static void draw_detail(void)
{
    unsigned int n;
    unsigned char i;

    if (!scr_wide) {
        if (gm_count == 0) {
            scr_field(2, 1, "", ENT_DATE_LEN, A_TEXT);
        } else {
            date_fmt(datebuf, gm_index[gm_sel].ts);
            scr_field(2, 1, datebuf, ENT_DATE_LEN, A_TEXT);
        }
    }

    if (gm_count == 0) {
        scr_rows_clear(DETAIL_ROW, DETAIL_ROW + 2);
        return;
    }

    if (scr_wide) {
        strcpy(detbuf, gm_index[gm_sel].subject);
        n = wrap_text(detbuf, wrapped[0], 2,
                      (unsigned char) (scr_cols - 4), sizeof(wrapped[0]));
        for (i = 0; i < 2; i++)
            scr_field((unsigned char) (DETAIL_ROW + i), 2,
                      (i < n) ? wrapped[i] : "",
                      (unsigned char) (scr_cols - 4), A_TEXT);

        strcpy(sbuf, "from ");
        strcat(sbuf, gm_index[gm_sel].name);
        scr_field(DETAIL_ROW + 2, 2, sbuf,
                  (unsigned char) (scr_cols - 4), A_TEXT);
    } else {
        strcpy(detbuf, gm_index[gm_sel].name);
        strcat(detbuf, ": ");
        strcat(detbuf, gm_index[gm_sel].subject);

        n = wrap_text(detbuf, wrapped[0], 3, scr_cols, sizeof(wrapped[0]));
        for (i = 0; i < 3; i++)
            scr_field((unsigned char) (DETAIL_ROW + i), 0,
                      (i < n) ? wrapped[i] : "", scr_cols, A_TEXT);
    }
}

void ui_inbox(void)
{
    unsigned char i;

    scr_clear();

    scr_field(0, 0, "", scr_cols, A_BAR);       /* the app bar */
    if (scr_wide) {
        logo_small(0, 1);
        scr_text(0, 8, "Gmail", A_BAR);
        scr_text(0, 16, "Inbox", A_BAR);
    } else {
        scr_text(0, 2, "Gmail  Inbox", A_BAR);
    }

    clk_col = right_col;
    ui_clock();

    page_indicator();
    scr_right(2, right_col, sbuf, A_TEXT);

    if (gm_count == 0) {
        scr_center(10, "No messages", A_TEXT);
    } else {
        for (i = 0; i < gm_count; i++)
            draw_entry(i);
    }

    scr_fill(RULE_ROW, 1, GL_RULE, (unsigned char) (scr_cols - 2), A_TEXT);
    draw_detail();

    footer(scr_wide
           ? "RET:READ   " GL_UP GL_DOWN ":MOVE   " GL_LEFT GL_RIGHT
             ":PAGE   C:COMPOSE   R:REFRESH   Q:QUIT"
           : "RET:READ " GL_LEFT GL_RIGHT ":PG C:NEW R:REFR Q:QUIT",
           0);
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

    scr_field(0, 0, "", scr_cols, A_BAR);
    strcpy(sbuf, "From: ");
    strcat(sbuf, gm_index[gm_sel].name);
    scr_text(0, 1, sbuf, A_BAR);

    /* Forty columns of "From:" and a 32-character name leave no room for a
       date on the bar; the wide layout has it where the Apple's does. */
    if (scr_wide) {
        date_fmt(datebuf, gm_index[gm_sel].ts);
        scr_right(0, right_col, datebuf, A_BAR);
        clk_col = (unsigned char) (right_col - ENT_DATE_LEN - 1);
    } else {
        clk_col = right_col;
    }
    ui_clock();

    nsub = wrap_text(gm_index[gm_sel].subject, wrapped[0], 2,
                     (unsigned char) (scr_cols - 2), sizeof(wrapped[0]));
    scr_field(1, 1, wrapped[0], (unsigned char) (scr_cols - 2), A_UNDER);
    scr_field(2, 1, (nsub > 1) ? wrapped[1] : "",
              (unsigned char) (scr_cols - 2), A_UNDER);

    if (gm_body_rows == 0) {
        scr_field(MSG_TOP, 1, "(no text content)",
                  (unsigned char) (scr_cols - 2), A_TEXT);
        for (i = 1; i < MSG_ROWS; i++)
            scr_row_clear((unsigned char) (MSG_TOP + i));
    } else {
        for (i = 0; i < MSG_ROWS; i++) {
            row = top + i;
            scr_field((unsigned char) (MSG_TOP + i), 1,
                      (row < gm_body_rows) ? gm_body[row] : "",
                      (unsigned char) (scr_cols - 2), A_TEXT);
        }
    }

    pages = (gm_body_rows + MSG_ROWS - 1) / MSG_ROWS;
    if (pages == 0)
        pages = 1;

    /* The last page is clamped flush against the end of the body, so its
       offset is not a whole multiple of a page. Report it as the last page
       anyway -- otherwise the bottom of a message reads "15/16" and looks
       like there is still somewhere to scroll. */
    if (gm_body_rows > MSG_ROWS && top >= gm_body_rows - MSG_ROWS)
        num(sbuf, pages);
    else
        num(sbuf, (unsigned int) (top / MSG_ROWS + 1));
    strcat(sbuf, "/");
    num(n, pages);
    strcat(sbuf, n);
    if (gm_body_trunc)
        strcat(sbuf, "+");

    footer(scr_wide
           ? "R:REPLY   F:FORWARD   " GL_UP GL_DOWN ":LINE   "
             GL_LEFT GL_RIGHT ":PAGE   ESC:BACK"
           : GL_UP GL_DOWN ":LN " GL_LEFT GL_RIGHT ":PG R:RPLY F:FWD ESC:BK",
           sbuf);
}

/* ------------------------------------------------------------------ */
/* Compose form                                                        */
/* ------------------------------------------------------------------ */

/*
 * One form serves both widths, the way every other screen here does: the
 * geometry comes off scr_cols, and only the label spelling and the footer
 * wording branch on scr_wide. The body lines store the 80-column shape and
 * window narrower at 40 -- the engine's horizontal scroll owns the
 * difference, exactly the GM_RT_COLS trade gm_body makes. The active field
 * is an A_SEL bar with the cursor cell knocked back to A_TEXT -- a hole in
 * the bar -- which reads on all three attribute tables, the MDA included.
 */

#define FRM_BODY_TOP    6
#define FRM_RULE_ROW    5
#define FRM_HINT_ROW    (FRM_BODY_TOP + FRM_VBODY + 1)
#define FRM_MSG_ROW     (FRM_HINT_ROW + 1)

#if FRM_MSG_ROW >= FOOT_ROW
#error "the form no longer fits above the footer -- lower FRM_VBODY"
#endif

static unsigned char frm_valcol(void)
{
    return (unsigned char) (scr_wide ? 10 : 6);
}

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
    return (f >= F_BODY0) ? 1 : frm_valcol();
}

/* Windows capped at one cell past their storage, so the cursor can sit
   past a full value; at 40 columns the screen is the cap and the engine
   scrolls the rest into view. */
unsigned char ui_form_width(unsigned char f)
{
    unsigned char room = (unsigned char) (scr_cols - frm_col(f) - 1);
    unsigned char most = (unsigned char)
        ((f >= F_BODY0) ? (FRM_BODY_COLS + 1) : (FRM_TO_MAX + 1));

    return (room < most) ? room : most;
}

void ui_form(unsigned char mode)
{
    const char *title = (mode == FRM_REPLY) ? "Reply"
                      : (mode == FRM_FWD)   ? "Forward"
                                            : "New message";

    scr_clear();

    scr_field(0, 0, "", scr_cols, A_BAR);       /* the app bar */
    if (scr_wide) {
        logo_small(0, 1);
        scr_text(0, 8, "Gmail", A_BAR);
        scr_text(0, 16, title, A_BAR);
    } else {
        scr_text(0, 2, "Gmail  ", A_BAR);
        scr_text(0, 9, title, A_BAR);
    }

    clk_col = right_col;
    ui_clock();

    scr_text(3, 2, "To", A_TEXT);
    scr_text(4, 2, scr_wide ? "Subject" : "Sub", A_TEXT);
    scr_fill(FRM_RULE_ROW, 1, GL_RULE, (unsigned char) (scr_cols - 2),
             A_TEXT);

    if (mode == FRM_REPLY)
        scr_text(FRM_HINT_ROW, 2, scr_wide
                 ? "Blank To and Subject take the reply defaults"
                 : "Blank To/Sub = reply defaults", A_TEXT);

    footer(scr_wide
           ? "TAB/RET:NEXT   " GL_UP GL_DOWN ":FIELD   "
             GL_LEFT GL_RIGHT ":CURSOR   ESC:DONE"
           : "RET:NEXT " GL_UP GL_DOWN ":FIELD ESC:DONE", 0);
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    unsigned char row = frm_row(f);
    unsigned char col = frm_col(f);

    scr_field(row, col, win, ui_form_width(f),
              (unsigned char) (active ? A_SEL : A_TEXT));

    if (active)
        scr_cell(row, (unsigned char) (col + curx),
                 curx < strlen(win) ? (unsigned char) win[curx] : ' ',
                 scr_attr_byte(A_TEXT));
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

    scr_center(FRM_MSG_ROW, s, A_EMPH);
}
