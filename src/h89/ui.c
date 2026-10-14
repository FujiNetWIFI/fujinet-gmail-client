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

/* Header: the mark at cols 1-4 rows 0-2, everything else right of it. */
#define LOGO_ROW        0
#define LOGO_COL        1
#define HDR_TEXT_COL    6
#define CLOCK_COL       (RIGHT_COL - 4)
#define STATUS_W        (CLOCK_COL - 1 - HDR_TEXT_COL)

/* Flat screens: the large mark, centred, and the text under it. */
#define SPLASH_LOGO_ROW 4
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_LARGE_COLS) / 2)
#define FLAT_TEXT_ROW   13
#define FLAT_PROG_ROW   17

/* The progress bar: the 25th line's on a banded screen, the flat one's
   under the text. */
#define BAR_KEY_COL     20
#define BAR_KEY_W       57
#define BAR_FLAT_W      50

/*
 * Inbox: a table. Row 3 is its top rule with the column names set into it,
 * rows 4-19 the sixteen entries, row 20 the bottom rule with the page in it,
 * and rows 21-23 the selection spelled out.
 *
 * Column 0 is the unread bullet, outside the bar, the way the calendar's
 * colour chips are; the bar starts at column 1 with the caret. Column lines
 * between the fields join the rules' tees, and on the selected row they are
 * drawn in the bar's video so the bar is one piece.
 */
#define TOP_RULE        CONTENT_TOP                     /* 3 */
#define LIST_TOP        (TOP_RULE + 1)                  /* 4 */
#define BOT_RULE        (LIST_TOP + IDX_MAX)            /* 20 */
#define DETAIL_ROW      (BOT_RULE + 1)                  /* rows 21..23 */

#if DETAIL_ROW + 2 > LAST_ROW
#error "the inbox no longer fits -- IDX_MAX is too large for the H19"
#endif

#define COL_MARK        0
#define COL_CARET       1
#define COL_DATE        3
#define W_DATE          (ENT_DATE_LEN - 1)              /* 12 */
#define SEP1            (COL_DATE + W_DATE + 1)         /* 16 */
#define COL_FROM        (SEP1 + 2)                      /* 18 */
#define W_FROM          20
#define SEP2            (COL_FROM + W_FROM + 1)         /* 39 */
#define COL_SUBJ        (SEP2 + 2)                      /* 41 */
#define W_SUBJ          (RIGHT_COL + 1 - COL_SUBJ)      /* 38 */

/*
 * Reader: the sender and the date in the bar, the subject on rows 1-2, and
 * the body in a frame from row 3 to the bottom, two columns in from each
 * side -- which is why BODY_COLS is 76 on this machine.
 */
#define MSG_BOX_TOP     CONTENT_TOP                     /* 3 */
#define MSG_TOP         (MSG_BOX_TOP + 1)               /* 4 */
#define MSG_BOT         (MSG_TOP + MSG_ROWS - 1)        /* 22 */
#define MSG_BOX_BOT     (MSG_BOT + 1)                   /* 23 */
#define MSG_COL         2

#if MSG_BOX_BOT > LAST_ROW
#error "the reader no longer fits -- lower MSG_ROWS"
#endif
#if BODY_COLS > SCR_COLS - 2 * MSG_COL
#error "BODY_COLS is wider than the reader's frame"
#endif

static char sbuf[96];
static char wrapped[2][SCR_COLS + 1];
static char datebuf[ENT_DATE_LEN];

/* A banded screen is up, so a busy message can go on it and the clock has
   somewhere to be. */
static unsigned char banded;

unsigned char ui_screen;

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void num(char *dst, unsigned int v)
{
    utoa(v, dst, 10);
}

/* Two digits, zero padded, straight into dst[0..1]. */
static void num2(char *dst, unsigned char v)
{
    dst[0] = (char) ('0' + (v / 10) % 10);
    dst[1] = (char) ('0' + v % 10);
}

/* "3 of 16" style counts into sbuf, after whatever is there. */
static void cat_num(unsigned int v)
{
    num(sbuf + strlen(sbuf), v);
}

/* ------------------------------------------------------------------ */
/* Banded chrome                                                       */
/* ------------------------------------------------------------------ */

static void ui_band(const char *title)
{
    scr_clear();
    scr_fill(0, 0, ' ' | REV, SCR_COLS);        /* the app bar */
    logo_small(LOGO_ROW, LOGO_COL);
    scr_text(0, HDR_TEXT_COL, title, 1);
    banded = 1;
}

/* The wall clock, five cells right-aligned on row 2 of a banded screen. */
void ui_clock(void)
{
    char t[6];

    if (!banded)
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
static void status_field(const char *s, unsigned char inv)
{
    scr_field(2, HDR_TEXT_COL, s, STATUS_W, inv);
}

/* ------------------------------------------------------------------ */
/* Flat screens                                                        */
/* ------------------------------------------------------------------ */

static void flat_screen(void)
{
    scr_clear();
    scr_keys(0);
    logo_large(SPLASH_LOGO_ROW, SPLASH_LOGO_COL);
    banded = 0;
    ui_screen = UI_FLAT;
}

/* A flat screen is up just before a bus call that may take a while, so it
   goes to the terminal now rather than at the next key read. */
void ui_splash(void)
{
    flat_screen();
    scr_center(FLAT_TEXT_ROW, "FujiNet Gmail", 0);
    scr_center(FLAT_TEXT_ROW + 2, "Looking for FujiNet", 0);
    h19_flush();
}

void ui_notfound(void)
{
    flat_screen();
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

    if (banded) {
        h19_cursor(H19_NONE, 0);        /* the form's, while it sends */
        status_field(s, 1);
        scr_keys((reason == BUSY_INDEX)
                 ? "Waiting for the FujiNet -- up to a minute for the inbox"
                 : "Waiting for the FujiNet");
    } else {
        flat_screen();
        scr_center(FLAT_TEXT_ROW, s, 0);
        if (reason == BUSY_INDEX)
            scr_center(FLAT_TEXT_ROW + 2, "up to a minute", 0);
    }

    h19_flush();
}

#ifdef GM_PROGRESS_UI
/*
 * The two halves of a fetch. The open is the long one -- the adapter signs
 * in to Google and gathers the whole page before it answers -- and the
 * records then come off the bus one at a time, which is what the bar counts.
 */
void ui_auth_phase(void)
{
    if (banded)
        status_field("Authorizing...", 1);
    else
        scr_center(FLAT_PROG_ROW, "Authorizing...", 0);

    h19_flush();
}

/* A run of reverse cells for what is done and the checkerboard for what is
   not, width cells from col. */
static void bar(unsigned char row, unsigned char col, unsigned char width,
                unsigned char current, unsigned char total)
{
    unsigned char fill;

    fill = (unsigned char) (((unsigned int) current * width) / total);
    if (fill > width)
        fill = width;

    scr_fill(row, col, ' ' | REV, fill);
    scr_fill(row, (unsigned char) (col + fill), G_CHECKER,
             (unsigned char) (width - fill));
}

void ui_fetch_progress(unsigned char current, unsigned char total)
{
    if (total == 0)
        return;

    strcpy(sbuf, "Loading ");
    cat_num(current);
    strcat(sbuf, " of ");
    cat_num(total);

    if (banded) {
        status_field("Loading messages...", 1);
        scr_keys(0);
        scr_text(KEY_ROW, 1, sbuf, 0);
        bar(KEY_ROW, BAR_KEY_COL, BAR_KEY_W, current, total);
    } else {
        scr_row_clear(FLAT_PROG_ROW);
        scr_center(FLAT_PROG_ROW, sbuf, 0);
        bar(FLAT_PROG_ROW + 1, (SCR_COLS - BAR_FLAT_W) / 2, BAR_FLAT_W,
            current, total);
    }

    h19_flush();
}
#endif

/*
 * In place on the form it was sent from, which is still the best picture of
 * what went: the caller paints its own screen back afterwards whatever this
 * does, so a flat screen in between would be a whole screen for nothing.
 */
void ui_sent(void)
{
    if (banded) {
        h19_cursor(H19_NONE, 0);
        status_field("Message sent", 1);
        scr_keys("Message sent -- press any key");
    } else {
        flat_screen();
        scr_center(FLAT_TEXT_ROW, "Message sent", 0);
        scr_center(FLAT_TEXT_ROW + 4, "Press any key", 0);
    }
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
        strcpy(sbuf, "Error ");
        cat_num(code);
        l1 = sbuf;
        break;
    }

    flat_screen();
    scr_center(FLAT_TEXT_ROW - 1, "Gmail error", 0);
    scr_center(FLAT_TEXT_ROW + 1, l1, 0);
    if (l2)
        scr_center(FLAT_TEXT_ROW + 2, l2, 0);

    /* The raw codes underneath, because "Gmail error" on its own is not
       something anyone can act on. sbuf may be l1, so they go in a buffer
       of their own. */
    {
        static char raw[40];

        strcpy(raw, gm_stage);
        strcat(raw, " code ");
        num(n, code);
        strcat(raw, n);
        strcat(raw, " dev ");
        num(n, gm_dev_ecode);
        strcat(raw, n);
        scr_center(FLAT_TEXT_ROW + 4, raw, 0);
    }

    scr_center(FLAT_TEXT_ROW + 7, "Press any key", 0);
}

/* ------------------------------------------------------------------ */
/* Inbox                                                               */
/* ------------------------------------------------------------------ */

/* A rule across the table, with `tee` where each column line meets it. */
static void table_rule(unsigned char row, unsigned char tee)
{
    scr_fill(row, 0, G_HLINE, SCR_COLS);
    scr_cell(row, SEP1, tee);
    scr_cell(row, SEP2, tee);
}

/* One entry on its row: the bullet outside the bar, then the bar. */
static void draw_entry(unsigned char slot)
{
    struct entry *e   = &gm_index[slot];
    unsigned char row = (unsigned char) (LIST_TOP + slot);
    unsigned char sel = (unsigned char) (slot == gm_sel);
    unsigned char a   = sel ? REV : 0;

    scr_cell(row, COL_MARK, e->unread ? G_BULLET : ' ');
    scr_cell(row, COL_CARET, sel ? (G_ARROW_R | REV) : ' ');
    scr_cell(row, COL_CARET + 1, ' ' | a);

    date_fmt(datebuf, e->ts);
    scr_field(row, COL_DATE, datebuf, W_DATE, sel);

    /* The column lines and the pad either side of them, in the bar's
       video, so the bar is one piece with the lines running through it. */
    scr_cell(row, SEP1 - 1, ' ' | a);
    scr_cell(row, SEP1, G_VLINE | a);
    scr_cell(row, SEP1 + 1, ' ' | a);
    scr_field(row, COL_FROM, e->name, W_FROM, sel);
    scr_cell(row, SEP2 - 1, ' ' | a);
    scr_cell(row, SEP2, G_VLINE | a);
    scr_cell(row, SEP2 + 1, ' ' | a);
    scr_field(row, COL_SUBJ, e->subject, W_SUBJ, sel);

    /* Column 79 stays clear, so the bar stops one short of the edge. */
    scr_cell(row, RIGHT_COL + 1, ' ');
}

/* A row with nothing on it but the column lines. */
static void table_blank(unsigned char row)
{
    scr_row_clear(row);
    scr_cell(row, SEP1, G_VLINE);
    scr_cell(row, SEP2, G_VLINE);
}

/*
 * The subject in full under the table -- the column truncates it to 38 of
 * its up to 127 characters -- and the sender's name on its own line.
 */
static void draw_detail(void)
{
    unsigned int  n;
    unsigned char i;

    if (gm_count == 0) {
        scr_rows_clear(DETAIL_ROW, DETAIL_ROW + 2);
        return;
    }

    n = wrap_text(gm_index[gm_sel].subject, wrapped[0], 2, SCR_COLS - 4,
                  SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (DETAIL_ROW + i), 2,
                  (i < n) ? wrapped[i] : "", SCR_COLS - 4, 0);

    strcpy(sbuf, "from ");
    strcat(sbuf, gm_index[gm_sel].name);
    scr_field(DETAIL_ROW + 2, 2, sbuf, SCR_COLS - 4, 0);
}

/* "1-16/137" into sbuf, or nothing for an empty page. */
static void page_indicator(void)
{
    char n[12];

    sbuf[0] = '\0';
    if (gm_count == 0)
        return;

    ultoa(gm_range + 1, sbuf, 10);
    strcat(sbuf, "-");
    ultoa(gm_range + gm_count, n, 10);
    strcat(sbuf, n);
    strcat(sbuf, "/");
    ultoa(gm_total, n, 10);
    strcat(sbuf, n);
}

void ui_inbox(void)
{
    unsigned char i, unread;
    unsigned char len;
    char n[12];

    ui_band("Gmail");
    ui_screen = UI_INBOX;

    /* The folder, as the calendar's active view tab: a notch of normal
       video in the bar. */
    scr_text(0, RIGHT_COL - 6, " Inbox ", 0);

    if (gm_total) {
        ultoa(gm_total, sbuf, 10);
        strcat(sbuf, gm_total == 1 ? " message" : " messages");
        scr_text(1, HDR_TEXT_COL, sbuf, 0);
    }

    unread = 0;
    for (i = 0; i < gm_count; i++)
        unread = (unsigned char) (unread + gm_index[i].unread);
    if (unread) {
        num(n, unread);
        strcpy(sbuf, n);
        strcat(sbuf, " unread on this page");
        status_field(sbuf, 0);
    }
    ui_clock();

    table_rule(TOP_RULE, G_TEE_DOWN);
    scr_rule(TOP_RULE, 1, SEP1 - 1, "Date", 1);
    scr_rule(TOP_RULE, SEP1 + 1, SEP2 - SEP1 - 1, "From", 1);
    scr_rule(TOP_RULE, SEP2 + 1, SCR_COLS - SEP2 - 1, "Subject", 1);

    for (i = 0; i < IDX_MAX; i++) {
        if (i < gm_count)
            draw_entry(i);
        else
            table_blank((unsigned char) (LIST_TOP + i));
    }
    if (gm_count == 0)
        scr_field(LIST_TOP + 6, COL_SUBJ, "No messages", W_SUBJ, 0);

    table_rule(BOT_RULE, G_TEE_UP);
    page_indicator();
    if (sbuf[0]) {
        len = (unsigned char) strlen(sbuf);
        scr_rule(BOT_RULE, (unsigned char) (SCR_COLS - len - 6),
                 (unsigned char) (len + 4), sbuf, 0);
    }

    draw_detail();

    scr_keys("[RETURN] Read  [^v] Move  [<>] Page  [BLUE] Compose  "
             "[GRAY] Refresh  [Q] Quit");
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

static unsigned int msg_shown;      /* the top the terminal is showing */

static void msg_body(unsigned int top)
{
    unsigned char i;
    unsigned int  row;

    if (gm_body_rows == 0) {
        scr_field(MSG_TOP, MSG_COL, "(no text content)", BODY_COLS, 0);
        for (i = 1; i < MSG_ROWS; i++)
            scr_field((unsigned char) (MSG_TOP + i), MSG_COL, "", BODY_COLS,
                      0);
        return;
    }

    for (i = 0; i < MSG_ROWS; i++) {
        row = top + i;
        scr_field((unsigned char) (MSG_TOP + i), MSG_COL,
                  (row < gm_body_rows) ? gm_body[row] : "", BODY_COLS, 0);
    }
}

/*
 * "3/12" set into the frame's bottom edge. The last page is clamped flush
 * against the end of the body, so its offset is not a whole multiple of a
 * page; it reports as the last page anyway, or the bottom of a message
 * would read "15/16" and look like there was still somewhere to scroll.
 */
static void msg_page(unsigned int top)
{
    unsigned int  pages;
    unsigned char len;
    char n[8];

    pages = (gm_body_rows + MSG_ROWS - 1) / MSG_ROWS;
    if (pages == 0)
        pages = 1;

    if (gm_body_rows > MSG_ROWS && top >= gm_body_rows - MSG_ROWS)
        num(sbuf, pages);
    else
        num(sbuf, (unsigned int) (top / MSG_ROWS + 1));
    strcat(sbuf, "/");
    num(n, pages);
    strcat(sbuf, n);
    if (gm_body_trunc)
        strcat(sbuf, "+");

    /* The frame's edge under it first, so a shorter label leaves no tail. */
    scr_fill(MSG_BOX_BOT, SCR_COLS - 16, G_HLINE, 15);
    len = (unsigned char) strlen(sbuf);
    scr_rule(MSG_BOX_BOT, (unsigned char) (SCR_COLS - len - 6),
             (unsigned char) (len + 4), sbuf, 0);
}

void ui_message(unsigned int top)
{
    unsigned int  nsub;
    unsigned char i;

    /*
     * A line either way with the reader already up: move the terminal's
     * lines with delete and insert line, then paint the body as usual. The
     * copy of the screen h19.c keeps moves with them, so the flush finds
     * every line but the new one already right.
     */
    if (ui_screen == UI_READER) {
        if (top == msg_shown + 1)
            h19_scroll(MSG_TOP, MSG_BOT, 1);
        else if (top + 1 == msg_shown)
            h19_scroll(MSG_TOP, MSG_BOT, -1);
        msg_body(top);
        msg_page(top);
        msg_shown = top;
        return;
    }

    strcpy(sbuf, "From: ");
    strcat(sbuf, gm_index[gm_sel].name);
    ui_band(sbuf);
    ui_screen = UI_READER;

    date_fmt(datebuf, gm_index[gm_sel].ts);
    scr_right(0, RIGHT_COL, datebuf, 1);

    nsub = wrap_text(gm_index[gm_sel].subject, wrapped[0], 2, STATUS_W,
                     SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (1 + i), HDR_TEXT_COL,
                  (i < nsub) ? wrapped[i] : "", STATUS_W, 0);
    ui_clock();

    scr_box(MSG_BOX_TOP, 0, MSG_BOX_BOT, SCR_COLS - 1);
    msg_body(top);
    msg_page(top);
    msg_shown = top;

    scr_keys("[^v] Line  [<>] Page  [RED] Reply  [F] Forward  [ESC] Back");
}

/* ------------------------------------------------------------------ */
/* Compose form                                                        */
/* ------------------------------------------------------------------ */

/*
 * The Apple's arrangement, framed: full-word labels and wide header
 * windows, and the body in a box like the reader's. An inactive header
 * shows its extent as an underline; the active field is a reverse bar with
 * the H19's own block cursor at the edit point, which the Apple has to fake
 * with a hole in the bar. compose.c owns the cursor and the horizontal
 * scroll; this end paints what it is given.
 */

#define FRM_VAL_COL     11
#define FRM_BOX_TOP     5
#define FRM_BODY_TOP    (FRM_BOX_TOP + 1)
#define FRM_BOX_BOT     (FRM_BODY_TOP + FRM_VBODY)
#define FRM_HINT_ROW    (FRM_BOX_BOT + 1)
#define FRM_MSG_ROW     (FRM_HINT_ROW + 1)

#if FRM_MSG_ROW > LAST_ROW
#error "the form no longer fits -- lower FRM_VBODY"
#endif
#if FRM_BODY_COLS + 1 > SCR_COLS - 2 * MSG_COL
#error "FRM_BODY_COLS is wider than the form's frame"
#endif

/* Where the cursor was put, so it comes back after the send? question. */
static unsigned char frm_cx, frm_cy;

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
    return (unsigned char) ((f >= F_BODY0) ? MSG_COL : FRM_VAL_COL);
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
    ui_band((mode == FRM_REPLY) ? "Reply"
          : (mode == FRM_FWD)   ? "Forward"
                                : "New message");
    ui_screen = UI_FORM;

    /* Row 1 says what a reply or a forward is acting on. */
    if (mode != FRM_COMPOSE) {
        scr_text(1, HDR_TEXT_COL,
                 (mode == FRM_REPLY) ? "In reply to" : "Forwarding", 0);
        scr_field(1, HDR_TEXT_COL + 12,
                  (mode == FRM_REPLY) ? gm_index[gm_sel].name
                                      : gm_index[gm_sel].subject,
                  STATUS_W - 12, 0);
    }
    ui_clock();

    scr_text(3, 2, "To", 0);
    scr_text(4, 2, "Subject", 0);

    scr_box(FRM_BOX_TOP, 0, FRM_BOX_BOT, SCR_COLS - 1);
    scr_rule(FRM_BOX_TOP, 1, SCR_COLS - 2, "Message", 1);

    if (mode == FRM_REPLY)
        scr_text(FRM_HINT_ROW, 2,
                 "Blank To and Subject take the reply defaults.", 0);

    scr_keys("[RETURN] Next  [^v] Field  [<>] Cursor  [DEL] Erase  "
             "[BLUE] Send  [ESC] Done");
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    unsigned char w = ui_form_width(f);
    unsigned char row = frm_row(f);
    unsigned char col = frm_col(f);
    unsigned char n;

    scr_field(row, col, win, w, active);

    if (active) {
        frm_cx = (unsigned char) (col + curx);
        frm_cy = row;
        h19_cursor(frm_cx, frm_cy);
        return;
    }

    /* The body is framed already; a header shows how far it goes. */
    if (f >= F_BODY0)
        return;
    n = (unsigned char) strlen(win);
    if (n < w)
        scr_fill(row, (unsigned char) (col + n), G_BAR_BOTTOM,
                 (unsigned char) (w - n));
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    scr_row_clear(FRM_MSG_ROW);

    switch (msg) {
    case FM_ASK:      s = " Send message? (Y/N) ";     break;
    case FM_NEEDTO:   s = " A recipient is required "; break;
    case FM_NEEDBODY: s = " A message is required ";   break;
    default:
        /* FM_NONE: cleared above, and the cursor back where it was. */
        h19_cursor(frm_cx, frm_cy);
        return;
    }

    /* The question is answered with a key, not typed into a field. */
    if (msg == FM_ASK)
        h19_cursor(H19_NONE, 0);

    scr_center(FRM_MSG_ROW, s, 1);
}
