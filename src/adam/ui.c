/*
 * Screen painters.
 *
 * Three screens: the inbox, the message reader, and the flat ones -- splash,
 * busy, error -- that share a layout and differ only in their text.
 *
 * Thirty-two columns, but twenty-one rows rather than the CoCo's sixteen, and
 * the difference is entirely the SmartKeys. Every other backend spends its
 * bottom row telling you what the keys do; this one has six labelled keys with
 * their captions already on the screen, so that row was never spent. It buys
 * the full portable IDX_MAX of sixteen entries per page -- the Atari's number
 * on a screen eight columns narrower -- where the CoCo has to come down to
 * eleven.
 *
 * Colour does the work that inverse video does elsewhere. There is no inverse
 * here, so a selected row is the same glyph under A_SEL, the app bar is a real
 * red band rather than a row of inverse spaces, and the unread column is a
 * solid red chip rather than an asterisk, a diamond or a semigraphics byte.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/* ------------------------------------------------------------------ */

/* List columns. Column 0 is the unread chip and is deliberately outside the
   selection bar: the chip's attribute byte *is* Gmail red and the bar's is
   gray, so a chip inside the bar would stop being the brand colour. The Atari
   keeps its column 0 out because an inverse space is COLPF1 and covers the
   player, the Apple because MouseText has no inverse form, the CoCo because
   XOR $40 on a semigraphics byte recolours it. Four machines, four unrelated
   reasons, one rule. */
#define COL_CHIP        0
#define COL_BAR         1                       /* the bar runs 1..31 */
#define COL_FROM        2
#define W_FROM          12
#define COL_SUBJ        15
#define W_SUBJ          (SCR_COLS - COL_SUBJ)   /* 17 */

/* The rule row under the app bar: the selected message's date on the left, the
   page indicator flush right. Both are about the list rather than in it. */
#define DATE_COL        0
#define W_DATE          (ENT_DATE_LEN - 1)      /* 12 */
#define W_PAGE          (SCR_COLS - W_DATE - 1) /* 19 */

/* Reader: sender and date on row 0. */
#define MSG_NAME_W      19                      /* cols 0-18 */
#define MSG_DATE_COL    (SCR_COLS - W_DATE)     /* cols 20-31 */

/* Flat screens. logo_large() takes the top-left of the frame, so the mark
   occupies rows 3-9 and columns 12-19. */
#define FLAT_LOGO_ROW   3
#define FLAT_LOGO_COL   ((SCR_COLS - ENV_FRAME_COLS) / 2)
#define FLAT_HEAD       11
#define FLAT_BODY       13
#define FLAT_CODES      16

static char sbuf[40];
static char nbuf[12];
static char detbuf[ENT_NAME_LEN + ENT_SUBJ_LEN + 4];
static char wrapped[2][SCR_COLS + 1];
static char datebuf[ENT_DATE_LEN];

/*
 * Where the wall clock goes on the screen that is up, or CLK_NONE.
 *
 * Three slots, because the three screens have three different spare cells.
 * The inbox and the form both paint a two-row app bar and only use the first
 * of them, so the clock takes the right-hand end of the second. The reader has
 * no app bar to spare -- row 0 is sender against date and rows 1-2 are the
 * subject -- so it goes on the rule row, to the left of the page indicator.
 *
 * The flat screens clear it: they are the ones up while AdamNet is running,
 * which is both the time nothing is pumping the clock and the time a repaint
 * is least welcome.
 */
#define CLK_NONE    0xFF
static unsigned char clk_row = CLK_NONE;
static unsigned char clk_col;
static unsigned char clk_attr;
static char          clkbuf[6];

/* ------------------------------------------------------------------ */
/* SmartKey legends                                                    */
/* ------------------------------------------------------------------ */

/*
 * Slots are 48, 40, 40, 40, 40 and 48 pixels wide, the font is proportional,
 * and smartkeys_puts() does not clip -- an overlong label is drawn straight
 * over its neighbour's slot. Every label here has been measured against
 * smartkeys_font[]; "Refresh" is the widest at 32 of its 40 pixels.
 *
 * The inbox spends slot V on "New" now that there is a compose to start;
 * the reader trades its Up/Down slots -- redundant with the arrow keys --
 * for Reply and Fwd, and keeps slot V NULL. A NULL slot is painted as
 * yellow status rather than as a keycap, which gives a band somewhere to
 * breathe where a screen can still afford it.
 */
static const struct sk_set sk_inbox = {
    { "Read", "Prev Pg", "Next Pg", "Refresh", "New", "Quit" },
    { K_ENTER, K_LEFT, K_RIGHT, K_REFRESH, K_COMPOSE, K_QUIT }
};

static const struct sk_set sk_reader = {
    { "Pg Up", "Pg Dn", "Reply", "Fwd", 0, "Back" },
    { K_LEFT, K_RIGHT, K_REPLY, K_FORWARD, K_NONE, K_BACK }
};

/*
 * The compose form's bank carries E_* editor codes, not K_* ones --
 * plat_getch() reads the same sk_key[] table sk_bind() fills, so the band
 * works in the form without any second mechanism. "Send" skips the
 * send-or-discard ask that "Done" poses when something was typed.
 */
static const struct sk_set sk_form = {
    { "Up", "Down", 0, 0, "Send", "Done" },
    { E_UP, E_DOWN, K_NONE, K_NONE, E_SAVE, E_DONE }
};

/* The flat screens all end in plat_anykey(), which takes anything at all, so
   the one legend is a courtesy rather than a map. */
static const struct sk_set sk_flat = {
    { 0, 0, 0, 0, 0, "OK" },
    { K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_ENTER }
};

/* ------------------------------------------------------------------ */
/* Flat screens                                                        */
/* ------------------------------------------------------------------ */

/* HH:MM, right-aligned at clk_col on clk_row. Hand-rolled rather than utoa'd
   because a leading zero is not optional in a clock. */
void ui_clock(void)
{
    if (clk_row == CLK_NONE || !gm_clock_ok)
        return;

    clkbuf[0] = (char) ('0' + gm_h / 10);
    clkbuf[1] = (char) ('0' + gm_h % 10);
    clkbuf[2] = ':';
    clkbuf[3] = (char) ('0' + gm_mi / 10);
    clkbuf[4] = (char) ('0' + gm_mi % 10);
    clkbuf[5] = '\0';

    scr_right(clk_row, clk_col, clkbuf, clk_attr);
}

static void flat_screen(void)
{
    clk_row = CLK_NONE;
    scr_clear();
    logo_large(FLAT_LOGO_ROW, FLAT_LOGO_COL);
    sk_bind(&sk_flat);
}

void ui_splash(void)
{
    flat_screen();
    scr_center(FLAT_HEAD, "FujiNet Gmail", A_BODY);
    scr_center(FLAT_BODY, "Waiting for FujiNet", A_DIM);
}

void ui_notfound(void)
{
    flat_screen();
    scr_center(FLAT_HEAD, "FujiNet not found", A_BODY);
    scr_center(FLAT_BODY, "Check the adapter", A_DIM);
}

void ui_busy(unsigned char reason)
{
    flat_screen();

    if (reason == BUSY_INDEX) {
        scr_center(FLAT_HEAD, "Opening mailbox...", A_BODY);
        scr_center(FLAT_BODY, "Up to 60 seconds", A_DIM);
    } else if (reason == BUSY_SEND) {
        scr_center(FLAT_HEAD, "Sending message...", A_BODY);
    } else {
        scr_center(FLAT_HEAD, "Fetching message...", A_BODY);
    }
}

void ui_sent(void)
{
    flat_screen();
    scr_center(FLAT_HEAD, "Message sent", A_BODY);
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
        l2 = "Re-authorize (scope)";
        break;
    case GM_NOTFOUND:
        l1 = "Message not found";
        l2 = "refresh the inbox";
        break;
    case GM_REJECTED:
        l1 = "Draft rejected";
        l2 = "Check the address";
        break;
    case GM_TOOBIG:
        l1 = "Message too large";
        break;
    case GM_NOSERVICE:
        l1 = "Service unavailable";
        l2 = "Check connection";
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
    scr_center(FLAT_HEAD, "Gmail error", A_BODY);
    scr_center(FLAT_BODY, l1, A_BODY);
    if (l2)
        scr_center((unsigned char) (FLAT_BODY + 1), l2, A_BODY);

    /* The raw codes underneath the friendly text. Worth the line: when
       something goes wrong on real hardware this is the difference between a
       reportable bug and "it just says error". It has to be built after the
       default branch above has finished with sbuf. */
    strcpy(sbuf, gm_stage);
    strcat(sbuf, " code ");
    utoa(code, nbuf, 10);
    strcat(sbuf, nbuf);
    strcat(sbuf, " dev ");
    utoa(gm_dev_ecode, nbuf, 10);
    strcat(sbuf, nbuf);
    scr_center(FLAT_CODES, sbuf, A_DIM);
}

/* ------------------------------------------------------------------ */
/* Inbox                                                               */
/* ------------------------------------------------------------------ */

/*
 * "1-16/137", flush right on the rule row.
 *
 * A mailbox big enough to outrun nineteen columns is not hypothetical -- six
 * digits of total is 17 characters in the long form -- and there is a date
 * immediately to the left for it to run into. So the form steps down rather
 * than overflowing: the range, then the position alone, then the size alone.
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
 * The rule row: the selected message's date, then the page indicator.
 *
 * There is no date column in the list -- From and Subject already truncate hard
 * at twelve and seventeen -- but there is exactly one message whose date
 * matters at any moment. It moves with the selection, so it reads as belonging
 * to the highlighted row. The Atari and the CoCo do the same thing for the same
 * reason.
 */
static void draw_rule(void)
{
    scr_field(RULE_ROW, DATE_COL, "", SCR_COLS, A_RULE);

    if (gm_count) {
        date_fmt(datebuf, gm_index[gm_sel].ts);
        scr_field(RULE_ROW, DATE_COL, datebuf, W_DATE, A_RULE);
    }

    page_indicator();
    scr_right(RULE_ROW, RIGHT_COL, sbuf, A_RULE);
}

/*
 * One list row.
 *
 * Column 0 is the chip and is painted rather than written: a solid Gmail red
 * cell for unread, plain page for read. The selection bar starts at column 1,
 * which also gives the chip an edge to read against on every row.
 */
static void draw_entry(unsigned char slot)
{
    struct entry *e    = &gm_index[slot];
    unsigned char row  = (unsigned char) (LIST_TOP + slot);
    unsigned char attr = (unsigned char) ((slot == gm_sel) ? A_SEL : A_BODY);

    scr_cell(row, COL_CHIP, e->unread ? GM_RED : G_PAPER);
    scr_field(row, COL_BAR, "", 1, attr);
    scr_field(row, COL_FROM, e->name, W_FROM, attr);
    scr_field(row, (unsigned char) (COL_FROM + W_FROM), "", 1, attr);
    scr_field(row, COL_SUBJ, e->subject, W_SUBJ, attr);
}

/*
 * The two rows under the list, which spell the selection out in full.
 *
 * They earn their keep here the way they do on the CoCo: sixty-four cells is
 * more of a name and subject than a 12-column From and a 17-column Subject
 * hold, and wrap_text() breaks it on a word.
 */
static void draw_panel(void)
{
    unsigned int  n;
    unsigned char i;

    if (gm_count == 0) {
        scr_rows_clear(PANEL_ROW, LAST_ROW);
        return;
    }

    strcpy(detbuf, gm_index[gm_sel].name);
    strcat(detbuf, ": ");
    strcat(detbuf, gm_index[gm_sel].subject);

    n = wrap_text(detbuf, (char *) wrapped, 2, SCR_COLS, SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (PANEL_ROW + i), 0,
                  (i < n) ? (const char *) wrapped[i] : "", SCR_COLS, A_DIM);
}

void ui_inbox(void)
{
    unsigned char i;

    scr_clear();

    /* The app bar, painted before the mark: logo_small() lays its white
       envelope over the two cells it occupies. */
    scr_field(0, 0, "", SCR_COLS, A_HEADER);
    scr_field(1, 0, "", SCR_COLS, A_HEADER);
    logo_small(LOGO_ROW, LOGO_COL);

    scr_text(0, HDR_TEXT_COL, "Gmail", A_HEADER);
    scr_right(0, RIGHT_COL, "Inbox", A_HDR_DIM);

    clk_row = 1;                /* the app bar's spare row */
    clk_col = RIGHT_COL;
    clk_attr = A_HDR_DIM;
    ui_clock();

    draw_rule();

    if (gm_count == 0)
        scr_center((unsigned char) (LIST_TOP + 7), "No messages", A_DIM);
    else
        for (i = 0; i < gm_count; i++)
            draw_entry(i);

    draw_panel();
    sk_bind(&sk_inbox);
}

/* Repaint only what a selection move touched. gm_sel is already the new one,
   and the page indicator is not a function of it -- but the date is, so the
   whole rule row is redrawn rather than half of it. */
void ui_inbox_sel(unsigned char from, unsigned char to)
{
    if (from < gm_count)
        draw_entry(from);
    if (to < gm_count)
        draw_entry(to);

    draw_rule();
    draw_panel();
}

/* ------------------------------------------------------------------ */
/* Message reader                                                      */
/* ------------------------------------------------------------------ */

/*
 * Row 0 is the sender and the date on the app bar's red, with no "From:"
 * label. The Atari and the Apple both spell the label out; six of thirty-two
 * columns is a fifth of the row to say something the top line of a mail reader
 * always means, and the date is worth more.
 *
 * There is no mark here. The reader wants the width for the sender, and two
 * cells of envelope would cost the date its last two columns.
 */
/*
 * There is no scr_clear() here, and that is deliberate: this is the one screen
 * repainted on every keystroke, and every one of our twenty-one rows is written
 * unconditionally below -- row 0 the sender and date, 1-2 the subject, 3 the
 * rule, and MSG_TOP + MSG_ROWS reaching LAST_ROW exactly. Clearing first would
 * add a full-screen wipe to every arrow press and show as a flash. The empty
 * body is the only path that does not fill the body rows, and it clears them
 * itself.
 */
void ui_message(unsigned int top)
{
    unsigned int  pages;
    unsigned int  row;
    unsigned int  nsub;
    unsigned char i;

    logo_hide();

    scr_field(MSG_HDR_ROW, 0, gm_index[gm_sel].name, MSG_NAME_W, A_HEADER);
    scr_field(MSG_HDR_ROW, MSG_NAME_W, "",
              (unsigned char) (MSG_DATE_COL - MSG_NAME_W), A_HEADER);
    date_fmt(datebuf, gm_index[gm_sel].ts);
    scr_field(MSG_HDR_ROW, MSG_DATE_COL, datebuf, W_DATE, A_HDR_DIM);

    nsub = wrap_text(gm_index[gm_sel].subject, (char *) wrapped, 2,
                     SCR_COLS, SCR_COLS + 1);
    scr_field(MSG_SUBJ_ROW, 0, wrapped[0], SCR_COLS, A_BODY);
    scr_field((unsigned char) (MSG_SUBJ_ROW + 1), 0,
              (nsub > 1) ? (const char *) wrapped[1] : "", SCR_COLS, A_BODY);

    if (gm_body_rows == 0) {
        scr_field(MSG_TOP, 0, "(no text content)", SCR_COLS, A_DIM);
        scr_rows_clear((unsigned char) (MSG_TOP + 1), LAST_ROW);
    } else {
        for (i = 0; i < MSG_ROWS; i++) {
            row = top + i;
            scr_field((unsigned char) (MSG_TOP + i), 0,
                      (row < gm_body_rows) ? (const char *) gm_body[row] : "",
                      SCR_COLS, A_BODY);
        }
    }

    /*
     * The rule row carries the page indicator, which is where it goes now that
     * there is no footer: MSG_TOP + MSG_ROWS is LAST_ROW + 1 exactly, so the
     * body runs to the bottom of our rows and there is nothing below it.
     */
    scr_field(MSG_RULE_ROW, 0, "", SCR_COLS, A_RULE);

    pages = (gm_body_rows + MSG_ROWS - 1) / MSG_ROWS;
    if (pages == 0)
        pages = 1;

    /* The last page is clamped flush against the end of the body, so its offset
       is not a whole multiple of a page. Report it as the last page anyway --
       otherwise the bottom of a message reads "15/16" and looks like there is
       still somewhere to scroll. */
    if (gm_body_rows > MSG_ROWS && top >= gm_body_rows - MSG_ROWS)
        utoa(pages, sbuf, 10);
    else
        utoa((unsigned int) (top / MSG_ROWS + 1), sbuf, 10);
    strcat(sbuf, "/");
    utoa(pages, nbuf, 10);
    strcat(sbuf, nbuf);
    if (gm_body_trunc)
        strcat(sbuf, "+");
    scr_right(MSG_RULE_ROW, RIGHT_COL, sbuf, A_RULE);

    /* On the rule row, clear of the page indicator: this screen has no spare
       app-bar row, and the widest indicator is "999/999+". */
    clk_row = MSG_RULE_ROW;
    clk_col = (unsigned char) (RIGHT_COL - 9);
    clk_attr = A_RULE;
    ui_clock();

    sk_bind(&sk_reader);
}

/* ------------------------------------------------------------------ */
/* Compose form                                                        */
/* ------------------------------------------------------------------ */

/*
 * The inbox's arrangement: the red app bar with the mode title, the rule
 * under it, gray captions beside black values, and the SmartKeys carrying
 * the actions -- sk_form's slots hold E_* codes, which is the whole trick.
 * The active field is the selection bar with the cursor cell knocked back
 * to the body attribute, a hole in the bar; scr_attr() can repaint that
 * one cell without touching the glyph under it, which no other backend
 * can. Rows 21-23 stay smartkeyslib's, as everywhere.
 */

#define FRM_BODY_TOP    6
#define FRM_HINT_ROW    (FRM_BODY_TOP + FRM_VBODY + 1)
#define FRM_MSG_ROW     (FRM_HINT_ROW + 1)
#define FRM_HDR_COL     3       /* TO/SUBJECT value column */
#define FRM_HDR_W       29      /* their window: cols 3..31 */

#if FRM_MSG_ROW > 20
#error "the form no longer fits above the SmartKeys band -- lower FRM_VBODY"
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
    return (unsigned char) ((f >= F_BODY0) ? 0 : FRM_HDR_COL);
}

unsigned char ui_form_width(unsigned char f)
{
    return (unsigned char) ((f >= F_BODY0) ? SCR_COLS : FRM_HDR_W);
}

void ui_form(unsigned char mode)
{
    scr_clear();
    sk_bind(&sk_form);

    scr_field(0, 0, "", SCR_COLS, A_HEADER);
    scr_field(1, 0, "", SCR_COLS, A_HEADER);
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, "Gmail", A_HEADER);
    scr_right(0, RIGHT_COL, (mode == FRM_REPLY) ? "Reply"
                          : (mode == FRM_FWD)   ? "Forward"
                                                : "New", A_HDR_DIM);

    scr_field(RULE_ROW, 0, "", SCR_COLS, A_RULE);

    clk_row = 1;                /* the app bar's spare row, as on the inbox */
    clk_col = RIGHT_COL;
    clk_attr = A_HDR_DIM;
    ui_clock();

    scr_text(3, 0, "To", A_DIM);
    scr_text(4, 0, "Su", A_DIM);

    if (mode == FRM_REPLY)
        scr_text(FRM_HINT_ROW, 0, "Blank To/Su = reply defaults", A_DIM);
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    unsigned char row = frm_row(f);
    unsigned char col = frm_col(f);

    scr_field(row, col, win, ui_form_width(f),
              (unsigned char) (active ? A_SEL : A_BODY));

    if (active)
        scr_attr(row, (unsigned char) (col + curx), 1, A_BODY);
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    scr_row_clear(FRM_MSG_ROW);

    switch (msg) {
    case FM_ASK:      s = "Send? (Y/N)";       break;
    case FM_NEEDTO:   s = "To is required";    break;
    case FM_NEEDBODY: s = "A body is required"; break;
    default:          return;                 /* FM_NONE: cleared above */
    }

    scr_field(FRM_MSG_ROW, 0, s, SCR_COLS, A_SEL);
}
