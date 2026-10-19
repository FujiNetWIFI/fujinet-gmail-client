/*
 * Cells into h19.c's copy of the screen.
 *
 * The same painters the Apple backend has -- a field, padded and truncated;
 * text, right-aligned text, centred text; a run of one glyph -- so ui.c
 * reads the same on both. What is different is underneath: the
 * Apple's write the text page, and these write a buffer that input.c sends
 * by difference before it waits for a key. Painting a row twice costs a few
 * hundred cycles; only what ends up different goes down the wire.
 *
 * The lines and the boxes are the H19's own graphics characters, which meet
 * in the middle of the cell and join up, so a frame is a frame and not a row
 * of dashes.
 */

#include <string.h>

#include "../gmail.h"
#include "platform.h"

/* A string byte as a cell: ASCII and the graphics escapes are already ROM
   indices, all but the vertical line (see platform.h). */
#define CELL(c) ((unsigned char) ((c) == HG_VLINE_C ? G_VLINE : (c)))

/* ------------------------------------------------------------------ */
/* Fields                                                              */
/* ------------------------------------------------------------------ */

void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv)
{
    unsigned char *p;
    unsigned char  a = inv ? REV : 0;

    if (row >= H19_ROWS || col >= SCR_COLS)
        return;
    if (width > SCR_COLS - col)
        width = (unsigned char) (SCR_COLS - col);

    /* Not h19_line(row) + col: sccz80 scales arithmetic on a call's pointer
       result as if it pointed at ints, and puts the text at twice the
       column. Through a variable it is a byte pointer again. */
    p = h19_line(row);
    p += col;
    while (width && *s) {
        *p++ = (unsigned char) (CELL((unsigned char) *s) | a);
        s++;
        width--;
    }
    while (width--)
        *p++ = (unsigned char) (' ' | a);
}

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv)
{
    scr_field(row, col, s, (unsigned char) strlen(s), inv);
}

/* Right-align s so that its last character lands on column rcol. */
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len > rcol + 1)
        len = (unsigned char) (rcol + 1);
    scr_field(row, (unsigned char) (rcol + 1 - len), s, len, inv);
}

void scr_center(unsigned char row, const char *s, unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len >= SCR_COLS)
        scr_field(row, 0, s, SCR_COLS, inv);
    else
        scr_field(row, (unsigned char) ((SCR_COLS - len) / 2), s, len, inv);
}

void scr_cell(unsigned char row, unsigned char col, unsigned char cell)
{
    if (row < H19_ROWS && col < SCR_COLS)
        h19_line(row)[col] = cell;
}

void scr_fill(unsigned char row, unsigned char col, unsigned char cell,
              unsigned char width)
{
    unsigned char *p;

    if (row >= H19_ROWS || col >= SCR_COLS)
        return;
    if (width > SCR_COLS - col)
        width = (unsigned char) (SCR_COLS - col);
    p = h19_line(row);                  /* see scr_field */
    memset(p + col, cell, width);
}

void scr_row_clear(unsigned char row)
{
    scr_fill(row, 0, ' ', SCR_COLS);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    unsigned char r;

    for (r = first; r <= last; r++)
        scr_row_clear(r);
}

void scr_clear(void)
{
    h19_begin();
}

/* ------------------------------------------------------------------ */
/* Lines and boxes                                                     */
/* ------------------------------------------------------------------ */

void scr_rule(unsigned char row, unsigned char col, unsigned char width,
              const char *label, unsigned char lcol)
{
    unsigned char len;

    scr_fill(row, col, G_HLINE, width);
    if (!label || !*label)
        return;

    /* "-| label |-": a tee each side and a space inside each, so the label
       reads as set into the line rather than written over it. */
    len = (unsigned char) strlen(label);
    if (lcol + len + 4 > width)
        return;
    col = (unsigned char) (col + lcol);
    scr_cell(row, col, G_TEE_LEFT);
    scr_cell(row, (unsigned char) (col + 1), ' ');
    scr_text(row, (unsigned char) (col + 2), label, 0);
    scr_cell(row, (unsigned char) (col + 2 + len), ' ');
    scr_cell(row, (unsigned char) (col + 3 + len), G_TEE_RIGHT);
}

void scr_box(unsigned char top, unsigned char left, unsigned char bottom,
             unsigned char right)
{
    unsigned char r;
    unsigned char w = (unsigned char) (right - left - 1);

    scr_cell(top, left, G_CORNER_UL);
    scr_fill(top, (unsigned char) (left + 1), G_HLINE, w);
    scr_cell(top, right, G_CORNER_UR);

    for (r = (unsigned char) (top + 1); r < bottom; r++) {
        scr_cell(r, left, G_VLINE);
        scr_cell(r, right, G_VLINE);
    }

    scr_cell(bottom, left, G_CORNER_LL);
    scr_fill(bottom, (unsigned char) (left + 1), G_HLINE, w);
    scr_cell(bottom, right, G_CORNER_LR);
}

/* ------------------------------------------------------------------ */
/* The 25th line                                                       */
/* ------------------------------------------------------------------ */

void scr_keys(const char *legend)
{
    unsigned char *p = h19_line(KEY_ROW);
    unsigned char  col = 1;
    unsigned char  a = 0;

    memset(p, ' ', SCR_COLS);
    if (!legend)
        return;

    for (; *legend && col < SCR_COLS - 1; legend++) {
        if (*legend == '[') {
            a = REV;
            continue;
        }
        if (*legend == ']') {
            a = 0;
            continue;
        }
        p[col++] = (unsigned char) (CELL((unsigned char) *legend) | a);
    }
}

/* ------------------------------------------------------------------ */
/* Platform lifecycle                                                  */
/* ------------------------------------------------------------------ */

void plat_init(void)
{
    h19_init();
}

void plat_shutdown(void)
{
    h19_exit();
}
