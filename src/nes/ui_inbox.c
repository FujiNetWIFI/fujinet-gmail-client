/*
 * ui_inbox.c -- the INBOX bank: the message list.
 *
 *   rows  0-1   the app bar: the mark, "Gmail", the clock; "Inbox" and the
 *               page indicator
 *         2-21  ten messages, two rows each: the sender with the date at the
 *               right, then the subject
 *        22-25  the selected message spelled out: a rule, the sender and
 *               date, the whole subject across two rows
 *        26-27  the hint bar
 *
 * A message is two rows because colour is: an attribute quadrant is two text
 * rows deep, so a one-row entry could not be highlighted without its
 * neighbour. Two rows also give 64 columns to the sender and 62 to the
 * subject, which is more than any other backend shows of either.
 *
 * Gmail's own look carries the rest. Unread mail is white with a dot at the
 * left; read mail is the same with grey lettering; the selection is Gmail's
 * pale blue. The bold Gmail uses for unread is the one thing a three-pixel
 * font cannot do, and the paper does that job instead.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"

#pragma code-name (push, "INBOX")
#pragma rodata-name (push, "INBOXRO")

#define LIST_TOP    2
#define PANEL_ROW   22
#define NAME_COL    2
#define NAME_W      47
#define DATE_COL    49
#define DATE_W      15


/* s right-aligned in the w cells from col. */
static void put_right(uint8_t row, uint8_t col, uint8_t w, const char *s)
{
    uint8_t n = (uint8_t) strlen(s);

    if (n > w)
        n = w;
    scr_put(row, col, "", (uint8_t) (w - n));
    scr_put(row, (uint8_t) (col + w - n), s, n);
}

static void entry(uint8_t i)
{
    struct entry *e = &gm_index[i];
    uint8_t       row = (uint8_t) (LIST_TOP + (i << 1));
    char          date[ENT_DATE_LEN];

    scr_band((uint8_t) (1 + i),
             (i == gm_sel) ? PAL_SEL : (e->unread ? PAL_PAPER : PAL_GREY));

    scr_put(row, 0, e->unread ? G_DOT : "", NAME_COL);
    scr_put(row, NAME_COL, e->name, NAME_W);
    date_fmt(date, e->ts);
    put_right(row, DATE_COL, DATE_W, date);

    scr_put((uint8_t) (row + 1), 0, "", NAME_COL);
    scr_put((uint8_t) (row + 1), NAME_COL, e->subject, SCR_COLS - NAME_COL);
}

static void blank_entry(uint8_t i)
{
    uint8_t row = (uint8_t) (LIST_TOP + (i << 1));

    scr_band((uint8_t) (1 + i), PAL_PAPER);
    scr_blank(row, (uint8_t) (row + 1));
}

/* "1-10 of 137", right of the title on the app bar's second row. */
static void page_indicator(void)
{
    if (gm_count == 0) {
        ui_buf[0] = '\0';
    } else {
        ultoa(gm_range + 1, ui_buf, 10);
        strcat(ui_buf, "-");
        ultoa(gm_range + gm_count, ui_num, 10);
        strcat(ui_buf, ui_num);
        strcat(ui_buf, " of ");
        ultoa(gm_total, ui_num, 10);
        strcat(ui_buf, ui_num);
    }
    put_right(1, 40, 23, ui_buf);
}

static void panel(void)
{
    struct entry *e = &gm_index[gm_sel];
    char          date[ENT_DATE_LEN];
    unsigned int  n;
    uint8_t       i;

    if (gm_count == 0) {
        scr_blank(PANEL_ROW, PANEL_ROW + 3);
        return;
    }

    memset(ui_buf, C_RULE, SCR_COLS);
    ui_buf[SCR_COLS] = '\0';
    scr_put(PANEL_ROW, 0, ui_buf, SCR_COLS);

    strcpy(ui_buf, "From: ");
    strcat(ui_buf, e->name);
    scr_put(PANEL_ROW + 1, 0, ui_buf, DATE_COL);
    date_fmt(date, e->ts);
    put_right(PANEL_ROW + 1, DATE_COL, DATE_W, date);

    n = wrap_text(e->subject, (char *) ui_wrap, 2, SCR_COLS, SCR_COLS + 1);
    for (i = 0; i < 2; ++i)
        scr_put((uint8_t) (PANEL_ROW + 2 + i), 0,
                (i < n) ? (const char *) ui_wrap[i] : "", SCR_COLS);
}

void inbox_paint(void)
{
    uint8_t i;

    ui_screen = SCREEN_INBOX;
    scr_begin();
    spr_clear();
    scr_cls();
    header_paint("Inbox");
    page_indicator();

    if (gm_count == 0)
        scr_center(LIST_TOP + 8, "No messages here");
    for (i = 0; i < IDX_MAX; ++i) {
        if (i < gm_count)
            entry(i);
        else if (gm_count)
            blank_entry(i);
    }

    panel();
    hint_paint("A Open    START Compose    SELECT Refresh    "
               G_UP G_DOWN " Move    " G_LEFT G_RIGHT " Page",
               "RETURN Open    C Compose    HOME Refresh    "
               "Arrows Move and page");
    scr_end();
}

/* A move: the two entries' colours, and the panel. The page indicator does
   not depend on the selection. */
void inbox_sel(uint8_t from, uint8_t to)
{
    if (from < gm_count)
        entry(from);
    if (to < gm_count)
        entry(to);
    panel();
}

