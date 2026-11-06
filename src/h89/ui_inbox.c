/*
 * The inbox.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"
#include "ui_int.h"

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

    date_fmt(ui_datebuf, e->ts);
    scr_field(row, COL_DATE, ui_datebuf, W_DATE, sel);

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

    n = wrap_text(gm_index[gm_sel].subject, ui_wrapped[0], 2, SCR_COLS - 4,
                  SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (DETAIL_ROW + i), 2,
                  (i < n) ? ui_wrapped[i] : "", SCR_COLS - 4, 0);

    strcpy(ui_sbuf, "from ");
    strcat(ui_sbuf, gm_index[gm_sel].name);
    scr_field(DETAIL_ROW + 2, 2, ui_sbuf, SCR_COLS - 4, 0);
}

/* "1-16/137" into ui_sbuf, or nothing for an empty page. */
static void page_indicator(void)
{
    char n[12];

    ui_sbuf[0] = '\0';
    if (gm_count == 0)
        return;

    ultoa(gm_range + 1, ui_sbuf, 10);
    strcat(ui_sbuf, "-");
    ultoa(gm_range + gm_count, n, 10);
    strcat(ui_sbuf, n);
    strcat(ui_sbuf, "/");
    ultoa(gm_total, n, 10);
    strcat(ui_sbuf, n);
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
        ultoa(gm_total, ui_sbuf, 10);
        strcat(ui_sbuf, gm_total == 1 ? " message" : " messages");
        scr_text(1, HDR_TEXT_COL, ui_sbuf, 0);
    }

    unread = 0;
    for (i = 0; i < gm_count; i++)
        unread = (unsigned char) (unread + gm_index[i].unread);
    if (unread) {
        ui_num(n, unread);
        strcpy(ui_sbuf, n);
        strcat(ui_sbuf, " unread on this page");
        ui_status(ui_sbuf, 0);
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
    if (ui_sbuf[0]) {
        len = (unsigned char) strlen(ui_sbuf);
        scr_rule(BOT_RULE, (unsigned char) (SCR_COLS - len - 6),
                 (unsigned char) (len + 4), ui_sbuf, 0);
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
