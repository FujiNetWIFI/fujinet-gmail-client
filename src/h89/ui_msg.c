/*
 * The message reader.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"
#include "ui_int.h"

/*
 * Reader: the sender and the date in the bar, the subject on rows 1-2, and
 * the body in a frame from row 3 to the bottom, two columns in from each
 * side -- which is why BODY_COLS is 76 on this machine.
 */
#define MSG_BOX_TOP     CONTENT_TOP                     /* 3 */
#define MSG_TOP         (MSG_BOX_TOP + 1)               /* 4 */
#define MSG_BOT         (MSG_TOP + MSG_ROWS - 1)        /* 22 */
#define MSG_BOX_BOT     (MSG_BOT + 1)                   /* 23 */

#if MSG_BOX_BOT > LAST_ROW
#error "the reader no longer fits -- lower MSG_ROWS"
#endif
#if BODY_COLS > SCR_COLS - 2 * MSG_COL
#error "BODY_COLS is wider than the reader's frame"
#endif

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
        ui_num(ui_sbuf, pages);
    else
        ui_num(ui_sbuf, (unsigned int) (top / MSG_ROWS + 1));
    strcat(ui_sbuf, "/");
    ui_num(n, pages);
    strcat(ui_sbuf, n);
    if (gm_body_trunc)
        strcat(ui_sbuf, "+");

    /* The frame's edge under it first, so a shorter label leaves no tail. */
    scr_fill(MSG_BOX_BOT, SCR_COLS - 16, G_HLINE, 15);
    len = (unsigned char) strlen(ui_sbuf);
    scr_rule(MSG_BOX_BOT, (unsigned char) (SCR_COLS - len - 6),
             (unsigned char) (len + 4), ui_sbuf, 0);
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

    strcpy(ui_sbuf, "From: ");
    strcat(ui_sbuf, gm_index[gm_sel].name);
    ui_band(ui_sbuf);
    ui_screen = UI_READER;

    date_fmt(ui_datebuf, gm_index[gm_sel].ts);
    scr_right(0, RIGHT_COL, ui_datebuf, 1);

    nsub = wrap_text(gm_index[gm_sel].subject, ui_wrapped[0], 2, STATUS_W,
                     SCR_COLS + 1);
    for (i = 0; i < 2; i++)
        scr_field((unsigned char) (1 + i), HDR_TEXT_COL,
                  (i < nsub) ? ui_wrapped[i] : "", STATUS_W, 0);
    ui_clock();

    scr_box(MSG_BOX_TOP, 0, MSG_BOX_BOT, SCR_COLS - 1);
    msg_body(top);
    msg_page(top);
    msg_shown = top;

    scr_keys("[^v] Line  [<>] Page  [RED] Reply  [F] Forward  [ESC] Back");
}
