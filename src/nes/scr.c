/*
 * scr.c -- the text screen as the painters see it: 28 rows of 64 characters
 * and fourteen two-row bands of colour (src/nes/neshw.h, ppu.s).
 *
 * Everything here only changes memory. A painter writes characters into
 * scr_txt and colours into att; what reaches the PPU, and when, is ppu.s's
 * business. That is what lets a painter repaint a row as often as it likes
 * and only pay for the cells that actually changed: scr_put compares as it
 * copies and marks just the span that differs.
 *
 * Colour is per attribute quadrant -- four columns by two rows -- which is
 * the one constraint every layout in this backend is built around. The text
 * rows pair up as (0,1), (2,3) ... (26,27), group g being rows 2g and 2g+1,
 * and a column span is coloured in fours.
 */

#include <string.h>

#include "../gmail.h"
#include "platform.h"

#define FRAME_LO    (*(volatile uint8_t *) &frame_count)

static const uint8_t att_bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

static void mark(uint8_t row, uint8_t t0, uint8_t t1)
{
    if (t0 < dirty_lo[row])
        dirty_lo[row] = t0;
    if (t1 > dirty_hi[row])
        dirty_hi[row] = t1;
}

void scr_put(uint8_t row, uint8_t col, const char *s, uint8_t w)
{
    char   *d = scr_txt + ((unsigned int) row << 6) + col;
    uint8_t i, c, lo = 0xFF, hi = 0;

    if (col + w > SCR_COLS)
        w = (uint8_t) (SCR_COLS - col);

    for (i = 0; i < w; ++i) {
        c = (uint8_t) *s;
        if (c)
            ++s;
        else
            c = ' ';
        c &= 0x7F;
        if ((uint8_t) d[i] != c) {
            d[i] = (char) c;
            if (lo == 0xFF)
                lo = i;
            hi = i;
        }
    }

    if (lo != 0xFF)
        mark(row, (uint8_t) ((col + lo) >> 1), (uint8_t) ((col + hi) >> 1));
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    scr_put(row, col, s, (uint8_t) strlen(s));
}

void scr_right(uint8_t row, uint8_t end, const char *s)
{
    uint8_t n = (uint8_t) strlen(s);

    if (n > end)
        n = end;
    scr_put(row, (uint8_t) (end - n), s, n);
}

void scr_center(uint8_t row, const char *s)
{
    uint8_t n = (uint8_t) strlen(s);

    if (n > SCR_COLS)
        n = SCR_COLS;
    scr_put(row, (uint8_t) ((SCR_COLS - n) >> 1), s, n);
}

void scr_blank(uint8_t row0, uint8_t row1)
{
    for (; row0 <= row1; ++row0)
        scr_put(row0, 0, "", SCR_COLS);
}

void scr_attr(uint8_t g, uint8_t q0, uint8_t q1, uint8_t p)
{
    uint8_t a   = (uint8_t) ((g + 1) >> 1);
    uint8_t sh0 = (uint8_t) ((g & 1) ? 0 : 4);
    uint8_t q, i, sh, v, changed = 0;

    for (q = q0; q <= q1; ++q) {
        i = (uint8_t) ((a << 3) + (q >> 1));
        sh = (uint8_t) (sh0 + ((q & 1) << 1));
        v = (uint8_t) ((att[i] & (uint8_t) ~(3 << sh)) | (p << sh));
        if (v != att[i]) {
            att[i] = v;
            changed = 1;
        }
    }

    if (changed)
        att_dirty |= att_bit[a];
}

void scr_band(uint8_t g, uint8_t p)
{
    scr_attr(g, 0, 15, p);
}

void scr_cls(void)
{
    uint8_t r;

    for (r = 0; r < SCR_ROWS; ++r)
        scr_put(r, 0, "", SCR_COLS);
    for (r = 0; r < SCR_GROUPS; ++r)
        scr_band(r, PAL_PAPER);
}

/* ------------------------------------------------------------------ */
/* Getting it to the PPU                                               */
/* ------------------------------------------------------------------ */

static uint8_t any_dirty(void)
{
    uint8_t r;

    for (r = 0; r < SCR_ROWS; ++r)
        if (dirty_lo[r] != 0xFF)
            return 1;
    return 0;
}

void scr_frame(void)
{
    uint8_t f = FRAME_LO;

    scr_compose();
    while (FRAME_LO == f)
        ;
}

/*
 * Everything changed so far, on the screen. With the screen off the program
 * does the copying itself, as fast as the CPU goes; with it on, it composes
 * and waits a frame at a time while the interrupts copy.
 */
void scr_sync(void)
{
    if (ppu_off) {
        for (;;) {
            scr_compose();
            if (!ppu_busy() && !any_dirty())
                return;
            ppu_drain(255);
        }
    }

    while (any_dirty() || ppu_busy())
        scr_frame();
}

/* The screen off, at the next vertical blank. */
void scr_begin(void)
{
    if (ppu_off)
        return;
    ppu_direct = 1;
    while (!ppu_off)
        ;
}

/*
 * Everything across, then the screen back on at the next vertical blank. The
 * order of the two stores matters: an NMI between them must find ppu_direct
 * already clear, or it would turn the screen off again and leave ppu_off set
 * with the screen on.
 */
void scr_end(void)
{
    scr_sync();
    ppu_direct = 0;
    ppu_off = 0;
}

/* ------------------------------------------------------------------ */
/* Far data                                                            */
/* ------------------------------------------------------------------ */

/*
 * Copy n bytes from a banked address to the PPU's current address. Only
 * with the screen off. Lives here, in the fixed half, because the bank it
 * reads from takes the window the caller's own code may be running in.
 */
void ppu_copy_far(uint8_t bank, const uint8_t *src, unsigned int n)
{
    uint8_t was = cur_bank;

    prg_r6(bank);
    while (n--)
        PPU_DATA = *src++;
    prg_r6(was);
}
