/*
 * neshw.h -- the NES as the Gmail client drives it: the hardware, the banks,
 * and the interface to crt0.s and ppu.s.
 *
 * The cart is the FujiNet NES cartridge (fujinet-firmware pico/nes) running
 * this image as an MMC3 board (mapper 4): 128K PRG, 128K CHR-RAM, 8K WRAM at
 * $6000 (src/nes/gmail-nes.cfg).
 *
 *   PRG  $8000-$9FFF  R6: one screen's bank at a time (BANK_* below)
 *        $A000-$BFFF  R7: bank 13, never switched -- the font, the form
 *        $C000-$FFFF  fixed: everything else
 *
 *   CHR (1K banks)
 *         0-11  the text screen's bitmap, rows 0-23: three 4K pattern
 *               tables, one per eight text rows ("bands" 0-2)
 *        12-13  band 3's bitmap, rows 24-27 (half a table: 128 tiles)
 *        14-15  band 3's other half: fixed tiles, $80 blank
 *        16-18  the sprites (8 x 16, odd tile numbers: the $1000 table)
 *           19  R5's resting bank
 *       20-127  the message body: 108K, 64 bytes a row, 16 rows a bank,
 *               reached through R5 (bodystore.c)
 *
 * The text screen is a bitmap. Every cell of name table rows 2-29 is its own
 * tile, so 64 columns of 4-pixel characters can be drawn into CHR-RAM
 * anywhere -- the tile a cell shows never changes, only its pixels do. That
 * is 896 tiles against a pattern table's 256, which is why MMC3's scanline
 * counter switches the background's table (R0/R1) every eight text rows.
 * Plane 1 of every bitmap tile is $FF and only plane 0 is ever drawn, so a
 * pixel is colour 2 (paper) or colour 3 (ink) and every colour on the screen
 * comes from the attribute table.
 */
#ifndef NESHW_H
#define NESHW_H

#include <stdint.h>

#define PPU_CTRL    (*(volatile uint8_t *)0x2000)
#define PPU_MASK    (*(volatile uint8_t *)0x2001)
#define PPU_STATUS  (*(volatile uint8_t *)0x2002)
#define PPU_SCROLL  (*(volatile uint8_t *)0x2005)
#define PPU_ADDR    (*(volatile uint8_t *)0x2006)
#define PPU_DATA    (*(volatile uint8_t *)0x2007)
#define JOY1        (*(volatile uint8_t *)0x4016)

#define MMC3_SEL    (*(volatile uint8_t *)0x8000)
#define MMC3_BANK   (*(volatile uint8_t *)0x8001)

/* PRG banks (8K) -- gmail-nes.cfg. */
#define BANK_BOOT       0
#define BANK_NET        1
#define BANK_INBOX      2
#define BANK_READER     3
#define BANK_COMPOSE    4
#define BANK_ART        5
#define BANK_R7         13

/* CHR banks (1K) -- see above. */
#define CHR_BAND3       12
#define CHR_FIXED       14
#define CHR_SPR         16
#define CHR_R5          19
#define CHR_BODY        20
#define CHR_BODY_END    128

/* The text screen. */
#define SCR_ROWS        28
#define SCR_COLS        64
#define SCR_TILES       32              /* tiles across a row */
#define SCR_GROUPS      14              /* two-row attribute groups */

/* Background palettes, the attribute values (ui_boot.c sets the colours). */
#define PAL_PAPER       0
#define PAL_SEL         1
#define PAL_GREY        2
#define PAL_RED         3

/* ---- crt0.s ------------------------------------------------------------ */

/* What the program last selected in MMC3's bank select register: the
   interrupts write the register themselves and put this back (crt0.s). */
extern uint8_t mmc3_sel;
#pragma zpsym ("mmc3_sel")

/* The R6 bank that is mapped. bank_tramp keeps it; prg_r6 sets it. */
extern uint8_t cur_bank;
#pragma zpsym ("cur_bank")
void __fastcall__ prg_r6(uint8_t bank);

/* The cross-bank call: see crt0.s. Used through #pragma wrapped-call only. */
void bank_tramp(void);

/* Frames since power on, counted by the NMI. */
extern volatile uint32_t frame_count;

/*
 * The display's two modes. Normally the screen is on, and everything that
 * changes it goes through the stage, the shadows and the body ring, which the
 * interrupts copy across in the blank (ppu.s). With ppu_direct set the NMI
 * turns the screen off at the next vertical blank -- ppu_off says it has --
 * and the main program may then write the PPU itself as fast as it likes:
 * that is how a whole new screen goes up in a few frames instead of thirty.
 */
extern volatile uint8_t ppu_direct;
#pragma zpsym ("ppu_direct")
extern volatile uint8_t ppu_off;
#pragma zpsym ("ppu_off")

/* The sprites, sent to the PPU every frame. */
extern uint8_t oam[256];

/* The busy indicator: nonzero and the NMI bobs sprites SPIN_FIRST.. in turn. */
extern volatile uint8_t spin_on;
#pragma zpsym ("spin_on")
extern uint8_t spin_y;              /* their resting line */
#pragma zpsym ("spin_y")

/* ---- ppu.s ------------------------------------------------------------- */

/* The text screen: 28 rows of 64 characters, and for each row the span of
   tiles (0-31) that differs from what the PPU has. dirty_lo[r] is $FF when
   row r is clean. */
extern char    scr_txt[SCR_ROWS * SCR_COLS];
extern uint8_t dirty_lo[SCR_ROWS];
extern uint8_t dirty_hi[SCR_ROWS];

/* The attribute table and palettes as they should be. att_dirty has a bit
   for each of the eight 8-byte attribute rows; pal_dirty is a flag. */
extern uint8_t att[64];
extern volatile uint8_t att_dirty;
extern uint8_t pal[32];
extern volatile uint8_t pal_dirty;

/* Render the first dirty row into the stage, if the stage is free. Returns
   nonzero when it did. */
uint8_t scr_compose(void);

/* Run the PPU work queued for the blank: the palette, the attributes, the
   body store's transfers and the stage, as far as the budget goes (255: all
   of it). Only with the PPU free -- from the interrupts, or from the main
   program while ppu_off. */
void __fastcall__ ppu_drain(uint8_t budget);

/* Is there anything left for ppu_drain to do? */
uint8_t ppu_busy(void);

/*
 * The body store's transfers (bodystore.c). Writes are a ring of four rows:
 * the main program fills slot bs_head and advances it, the drain empties
 * bs_tail. A read is one row at a time: fill bs_rd_bank/_hi/_lo, set
 * bs_rd_state to 1, and the drain sets it to 2 with the row in bs_rd_buf.
 */
#define BS_RING 4
extern uint8_t bs_wbuf[BS_RING][64];
extern uint8_t bs_wbank[BS_RING], bs_whi[BS_RING], bs_wlo[BS_RING];
extern volatile uint8_t bs_head, bs_tail;
extern uint8_t bs_rd_buf[64];
extern uint8_t bs_rd_bank, bs_rd_hi, bs_rd_lo;
extern volatile uint8_t bs_rd_state;

#endif
