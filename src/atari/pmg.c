/*
 * The Gmail logo, in player/missile graphics.
 *
 * Four players, one per stroke of the "M", each carrying one of Gmail's four
 * colors. Players are only eight bits wide, so instead of tiling them on an
 * eight-bit grid they are positioned independently by HPOS so the strokes
 * touch: three bit-columns of blue pillar, eight of red diagonal descending,
 * eight of yellow diagonal descending the other way to meet it, three of green
 * pillar. At double width each bit is two color clocks, which makes the whole
 * mark 44 color clocks -- eleven character cells -- wide.
 *
 *      |\        /|
 *      | \      / |          blue  red      yellow  green
 *      |  \    /  |          pillar diagonal diagonal pillar
 *      |   \  /   |
 *      |    \/    |
 *
 * Priority runs lowest player index first, so the red and yellow diagonals
 * draw over the pillars where they meet -- the fold in the real mark.
 */

#include <string.h>

#include <atari.h>
#include <peekpoke.h>

#include "../gmail.h"
#include "platform.h"

/* Double-line resolution: 1K buffer, 128 bytes per player. */
#define PM_P0_OFF       0x200
#define PM_PSTRIDE      0x80
#define PM_BUFSZ        1024

/*
 * Screen-space mapping. Both of these are the standard values for a normal
 * width, 192 scanline playfield in double-line P/M resolution; they are the
 * two numbers to nudge if the logo ever lands off by a row or a cell.
 */
#define PM_TOP          16      /* player byte holding the first scanline of row 0 */
#define PM_ROWBYTES     4       /* double-line bytes per 8-scanline text row */
#define PM_LEFT         48      /* HPOS of screen column 0, in color clocks */
#define PM_COLCLK       4       /* color clocks per character cell */

#define PM_ROW(r)       (PM_TOP + PM_ROWBYTES * (r))
#define PM_COL(c)       (PM_LEFT + PM_COLCLK * (c))

#define HPOSP0          0xD000

static unsigned char *pmbase;
static unsigned char  pm_ok;

/* Player 0 red diagonal, 1 yellow diagonal, 2 blue pillar, 3 green pillar --
   the same stroke-to-color assignment the Intellivision version used. */
static const unsigned char pl_hoff[4] = { 6, 22, 0, 38 };

/* Four text rows tall: splash, status and error screens. */
static const unsigned char shape_large[4][16] = {
    { 0xC0, 0xC0, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18,
      0x0C, 0x0C, 0x06, 0x06, 0x03, 0x03, 0x01, 0x01 },   /* red   \ */
    { 0x03, 0x03, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18,
      0x30, 0x30, 0x60, 0x60, 0xC0, 0xC0, 0x80, 0x80 },   /* yellow / */
    { 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,
      0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0 },   /* blue  | */
    { 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,
      0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0 }    /* green | */
};

/* Two text rows tall: the inbox header. */
static const unsigned char shape_small[4][8] = {
    { 0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01 },
    { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80 },
    { 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0 },
    { 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0 }
};

void pmg_init(void)
{
    unsigned int lo = (unsigned int) OS.appmhi;
    unsigned int hi = (unsigned int) OS.memtop;
    unsigned int base;

    pm_ok = 0;

    /*
     * crt0 sets both APPMHI and the C stack pointer to MEMTOP minus
     * __RESERVED_MEMORY__ (2048, from LDFLAGS_EXTRA_ATARI in the top-level
     * Makefile) and the stack grows down from there, so everything from
     * APPMHI up to MEMTOP is ours. Any 2K window contains a whole 1K-aligned
     * 1K block, which is what the P/M buffer needs.
     *
     * Reading APPMHI rather than hardcoding the reserve size means a change to
     * the Makefile cannot silently put the buffer on top of the stack.
     */
    if (hi <= lo)
        return;

    base = (lo + 0x3FF) & 0xFC00;
    if (base < lo || hi < base || (unsigned int) (hi - base) < PM_BUFSZ)
        return;

    pmbase = (unsigned char *) base;
    memset(pmbase, 0, PM_BUFSZ);

    ANTIC.pmbase = (unsigned char) (base >> 8);

    OS.pcolr0 = C_LOGO_RED;
    OS.pcolr1 = C_LOGO_YELLOW;
    OS.pcolr2 = C_LOGO_BLUE;
    OS.pcolr3 = C_LOGO_GREEN;

    GTIA_WRITE.sizep0 = PMG_SIZE_DOUBLE;
    GTIA_WRITE.sizep1 = PMG_SIZE_DOUBLE;
    GTIA_WRITE.sizep2 = PMG_SIZE_DOUBLE;
    GTIA_WRITE.sizep3 = PMG_SIZE_DOUBLE;

    /* GPRIOR and SDMCTL are shadowed -- the vertical blank copies them into
       the hardware every frame, so write the shadow. GRACTL and PMBASE have no
       shadow and are written directly. */
    OS.gprior = PRIOR_P03_PF03;
    OS.sdmctl = DMACTL_DMA_FETCH | DMACTL_PLAYFIELD_NORMAL | DMACTL_DMA_PLAYERS;
    GTIA_WRITE.gractl = GRACTL_PLAYERS;

    pm_ok = 1;
}

void pmg_show(unsigned char variant, unsigned char row, unsigned char col)
{
    const unsigned char *sh;
    unsigned char *p;
    unsigned char  i;
    unsigned char  rows;
    unsigned char  x;

    if (!pm_ok)
        return;

    rows = (variant == LOGO_LARGE) ? 16 : 8;
    x = PM_COL(col);

    for (i = 0; i < 4; i++) {
        p = pmbase + PM_P0_OFF + (unsigned int) i * PM_PSTRIDE;
        memset(p, 0, PM_PSTRIDE);

        sh = (variant == LOGO_LARGE) ? shape_large[i] : shape_small[i];
        memcpy(p + PM_ROW(row), sh, rows);

        POKE(HPOSP0 + i, (unsigned char) (x + pl_hoff[i]));
    }
}

void pmg_hide(void)
{
    if (!pm_ok)
        return;

    /* Blank the shapes rather than touching GRACTL, so callers can hide
       unconditionally without worrying about the DMA state. */
    memset(pmbase + PM_P0_OFF, 0, 4 * PM_PSTRIDE);
}
