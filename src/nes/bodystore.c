/*
 * bodystore.c -- the message body, kept in CHR-RAM (gmail.h's GM_FAR_BODY).
 *
 * The CPU has 8K of cartridge RAM for everything, and a long message wants
 * ten times that. The cartridge's CHR-RAM is 128K, of which the screen and
 * the sprites need 20K; the other 108K holds the body, 64 bytes a row (the
 * wrap width, with no terminator), sixteen rows to each 1K bank -- 1728
 * rows, BODY_ROWS.
 *
 * The CPU reaches CHR-RAM only through the PPU, and only while the PPU is not
 * drawing. A row's bank goes into R5, the 1K window at $1C00 that no sprite
 * ever draws from (sprites.c keeps every pattern below $1C00), and the row is
 * copied through $2006/$2007 in the blank by ppu_drain (ppu.s): writes
 * through a ring of four, reads one row at a time. A body arrives with the
 * busy screen up and the screen on, so the ring is what keeps body_put_rows
 * from waiting a whole frame for every row. With the screen off the program
 * runs the drain itself and never waits at all.
 */

#include <string.h>

#include "../gmail.h"
#include "platform.h"

#if BODY_ROWS > (CHR_BODY_END - CHR_BODY) * 16
#error "BODY_ROWS is more rows than the CHR-RAM body store holds"
#endif
#if BODY_COLS != 64
#error "the body store's rows are 64 bytes: BODY_COLS must be 64"
#endif

static void where(unsigned int r, uint8_t *bank, uint8_t *hi, uint8_t *lo)
{
    uint8_t k = (uint8_t) (r & 15);

    *bank = (uint8_t) (CHR_BODY + (r >> 4));
    *hi = (uint8_t) (0x1C + (k >> 2));
    *lo = (uint8_t) ((k & 3) << 6);
}

void body_put_rows(unsigned int r, const char *rows, unsigned char n)
{
    uint8_t  h, len;
    uint8_t *d;

    for (; n; --n, ++r, rows += BODY_STRIDE) {
        /* wait for a free slot: the drain empties one or more a frame */
        while (((bs_head + 1) & (BS_RING - 1)) == bs_tail)
            if (ppu_off)
                ppu_drain(255);

        h = bs_head;
        d = bs_wbuf[h];
        len = (uint8_t) strlen(rows);
        memcpy(d, rows, len);
        memset(d + len, 0, (uint8_t) (64 - len));
        where(r, &bs_wbank[h], &bs_whi[h], &bs_wlo[h]);
        bs_head = (uint8_t) ((h + 1) & (BS_RING - 1));
    }

    if (ppu_off)
        ppu_drain(255);
}

const char *body_row(unsigned int r)
{
    static char out[BODY_STRIDE];

    where(r, &bs_rd_bank, &bs_rd_hi, &bs_rd_lo);
    bs_rd_state = 1;
    while (bs_rd_state != 2)
        if (ppu_off)
            ppu_drain(255);

    memcpy(out, bs_rd_buf, 64);
    out[64] = '\0';
    bs_rd_state = 0;
    return out;
}
