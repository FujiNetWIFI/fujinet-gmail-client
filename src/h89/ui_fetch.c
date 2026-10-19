/*
 * The progress of an inbox fetch, on the screen that asked for it.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"
#include "ui_int.h"

/* The progress bar: the 25th line's on a banded screen, the flat one's
   under the text. */
#define BAR_KEY_COL     20
#define BAR_KEY_W       57
#define BAR_FLAT_W      50

#ifdef GM_PROGRESS_UI
/*
 * The two halves of a fetch. The open is the long one -- the adapter signs
 * in to Google and gathers the whole page before it answers -- and the
 * records then come off the bus one at a time, which is what the bar counts.
 */
void ui_auth_phase(void)
{
    if (ui_banded)
        ui_status("Authorizing...", 1);
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

    strcpy(ui_sbuf, "Loading ");
    ui_cat_num(current);
    strcat(ui_sbuf, " of ");
    ui_cat_num(total);

    if (ui_banded) {
        ui_status("Loading messages...", 1);
        scr_keys(0);
        scr_text(KEY_ROW, 1, ui_sbuf, 0);
        bar(KEY_ROW, BAR_KEY_COL, BAR_KEY_W, current, total);
    } else {
        scr_row_clear(FLAT_PROG_ROW);
        scr_center(FLAT_PROG_ROW, ui_sbuf, 0);
        bar(FLAT_PROG_ROW + 1, (SCR_COLS - BAR_FLAT_W) / 2, BAR_FLAT_W,
            current, total);
    }

    h19_flush();
}
#endif
