/*
 * Greedy word wrap.
 *
 * Wraps one source line into a fixed-stride row array. Raw body text is never
 * stored: each line coming off the wire is wrapped into display rows on
 * arrival and the original is discarded, which is what keeps the body buffer
 * to a predictable size.
 *
 * An empty source line yields one empty row, so paragraph breaks survive.
 */

#include <string.h>

#include "gmail.h"

/*
 * Terminate a row, dropping any separator space that ended up against the
 * wrap point. Returns the trimmed length, which the ellipsis path needs.
 */
static unsigned char endrow(char *r, unsigned char col)
{
    while (col > 0 && r[col - 1] == ' ')
        col--;
    r[col] = '\0';
    return col;
}

unsigned int wrap_text(const char *src, char *rows, unsigned int max_rows,
                       unsigned char cols, unsigned char stride)
{
    unsigned int  row = 0;
    unsigned char col = 0;
    char         *r;
    const char   *w;
    unsigned char wl;
    unsigned char n;

    if (max_rows == 0 || cols == 0)
        return 0;

    r = rows;
    r[0] = '\0';

    for (;;) {
        /* Leading spaces are dropped at the start of a row; mid-row a single
           separator survives. */
        while (*src == ' ' && col == 0)
            src++;

        if (*src == '\0')
            break;

        if (*src == ' ') {
            src++;
            if (col < cols)
                r[col++] = ' ';
            continue;
        }

        /* Measure the next word. */
        w = src;
        wl = 0;
        while (*src != '\0' && *src != ' ' && wl < 254) {
            src++;
            wl++;
        }

        if (wl > cols) {
            /* Longer than a whole row -- hard-split it, starting from
               wherever we happen to be. */
            while (wl) {
                if (col >= cols) {
                    col = endrow(r, col);
                    if (++row >= max_rows)
                        goto overflow;
                    r = rows + row * stride;
                    col = 0;
                }
                n = cols - col;
                if (n > wl)
                    n = wl;
                memcpy(r + col, w, n);
                col += n;
                w += n;
                wl -= n;
            }
            continue;
        }

        if ((unsigned int) col + wl > cols) {
            col = endrow(r, col);
            if (++row >= max_rows)
                goto overflow;
            r = rows + row * stride;
            col = 0;
        }

        memcpy(r + col, w, wl);
        col += wl;
    }

    endrow(r, col);
    return row + 1;

overflow:
    /* r/col still describe the last row we completed. Ellipsize it in place
       rather than dropping the tail silently. */
    if (cols >= 3) {
        if (col > cols - 3)
            col = cols - 3;
        r[col++] = '.';
        r[col++] = '.';
        r[col++] = '.';
    }
    r[col] = '\0';
    return max_rows;
}
