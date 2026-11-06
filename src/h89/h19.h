/**
 * @file h19.h
 * @brief The H19 terminal's screen, drawn by cells and sent by difference.
 *
 * From fujinet-config's src/h89 (add-h89, 92cca0e), by way of the Google
 * Calendar client's H89 port (add-h89, c46d930). Changed on the way: the
 * selection bar and the text writer are gone (screen.c draws both into the
 * rows itself), and h19_line(), h19_scroll() and the yielding flush are new.
 *
 * A cell is one byte: bit 7 is reverse video and the low seven bits are the
 * H19's own character ROM index - 0x00-0x1F the graphics characters, 0x7F the
 * graphics bullet, 0x20-0x7E ASCII. Screens draw into a buffer; h19_flush()
 * sends only the cells that differ from what the terminal shows, with as few
 * cursor-address (ESC Y), reverse (ESC p/q) and graphics (ESC F/G) changes as
 * it can. The console runs at 9600 baud: about a thousand cells a second.
 */

#ifndef H89_H19_H
#define H89_H19_H

#include <stdint.h>

#define H19_COLS 80
#define H19_ROWS 25          /* 24 lines and the 25th */
#define H19_STATUS_ROW 24

#define REV 0x80

/* H19 graphics characters: their ROM index, shown in graphics mode (ESC F)
   for the character in the comment */
#define G_VLINE      0x00 /* `  vertical line */
#define G_HLINE      0x01 /* a  horizontal line */
#define G_CROSS      0x02 /* b  cross */
#define G_CORNER_UR  0x03 /* c  upper right corner */
#define G_CORNER_LR  0x04 /* d  lower right corner */
#define G_CORNER_LL  0x05 /* e  lower left corner */
#define G_CORNER_UL  0x06 /* f  upper left corner */
#define G_PLUSMINUS  0x07 /* g */
#define G_ARROW_R    0x08 /* h  right arrow */
#define G_CHECKER    0x09 /* i  checkerboard */
#define G_DIVIDE     0x0A /* j */
#define G_ARROW_D    0x0B /* k  down arrow */
#define G_QUAD_LR    0x0C /* l  lower right quadrant */
#define G_QUAD_LL    0x0D /* m  lower left quadrant */
#define G_QUAD_UL    0x0E /* n  upper left quadrant */
#define G_QUAD_UR    0x0F /* o  upper right quadrant */
#define G_HALF_TOP   0x10 /* p  upper half */
#define G_HALF_RIGHT 0x11 /* q  right half */
#define G_TRI_UL     0x12 /* r  upper left triangle */
#define G_TEE_DOWN   0x13 /* s  T, stem down */
#define G_TEE_LEFT   0x14 /* t  T, stem left */
#define G_TEE_UP     0x15 /* u  T, stem up */
#define G_TEE_RIGHT  0x16 /* v  T, stem right */
#define G_X          0x17 /* w  diagonal cross */
#define G_SLASH      0x18 /* x  / */
#define G_BACKSLASH  0x19 /* y  backslash */
#define G_BAR_TOP    0x1A /* z  top line */
#define G_BAR_BOTTOM 0x1B /* {  bottom line */
#define G_BAR_LEFT   0x1C /* |  left bar */
#define G_BAR_RIGHT  0x1D /* }  right bar */
#define G_PILCROW    0x1E /* ~ */
#define G_TRI_UR     0x1F /* _  upper right triangle */
#define G_BULLET     0x7F /* ^  bullet */

#define H19_NONE 0xFF

/** Set the terminal up (25th line on, cursor off and a block, keypad
    shifted, no wrap) */
void h19_init(void);

/** Put the terminal and the OS's console back; safe to call twice */
void h19_exit(void);

/** Blank lines 0-23 (not the 25th) and hide the cursor */
void h19_begin(void);

/** Line y of the screen being drawn, marked as changed, to write cells into */
uint8_t *h19_line(uint8_t y);

/** Scroll lines y0-y1 of the terminal by one, up (dir > 0: y0 leaves, y1
    comes in blank) or down, with delete and insert line. Only the terminal
    and what we know of it move: the screen being drawn is the caller's, and
    the next flush sends just the line that came in. */
void h19_scroll(uint8_t y0, uint8_t y1, int8_t dir);

/** Show the cursor at x,y after each flush; x = H19_NONE hides it */
void h19_cursor(uint8_t x, uint8_t y);

/** Send everything that changed */
void h19_flush(void);

/** The same, but stop between lines when a key is waiting: the screen the
    key leads to will be drawn over this one anyway. The rest goes next time. */
void h19_flush_poll(void);

/** Ring the terminal's bell */
void h19_bell(void);

#endif /* H89_H19_H */
