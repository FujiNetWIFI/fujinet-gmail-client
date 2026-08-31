/*
 * Video probe, attribute tables, and the blitter.
 *
 * The screen is whatever the machine was showing when GMAIL was typed: mode
 * 0 or 1 is 40x25, mode 2 or 3 is 80x25, mode 7 is the MDA. The probe reads
 * the current mode from the BIOS, remembers it for plat_shutdown(), and only
 * changes it when asked to (/40, /80) or forced to (a graphics mode, or
 * EGA/VGA talked into 43/50 rows -- both go back to mode 3 rather than
 * teaching ui.c a geometry nothing else has).
 *
 * Cells are written straight into the text page at B800:0000 -- B000:0000
 * for the MDA -- rather than through the BIOS. INT 10h writes one cell per
 * two interrupts (position, then write); a full 80x25 repaint is 4,000 of
 * them, which is visible on a 4.77 MHz 8088, and this program repaints whole
 * screens on every page turn. The one machine direct writes upset is the
 * genuine IBM CGA, which snows in 80-column text; /SNOW gates every write on
 * the retrace for that card. A switch rather than a heuristic because there
 * is no reliable way to detect a true CGA, snow is cosmetic, and the PCjr,
 * the MDA and virtually every clone would otherwise pay the wait for a fault
 * they do not have -- which is also how the software of the era shipped it.
 *
 * Command tail: /40 and /80 pick a width (and force the mode change), /MONO
 * keeps the black-and-white attribute table on a colour adapter -- for the
 * LCD and composite screens that render colour as mud -- and /SNOW is the
 * CGA gate above. Parsed from the PSP because the portable main() takes no
 * arguments.
 */

#include <conio.h>
#include <dos.h>
#include <stdlib.h>

#include "../gmail.h"
#include "platform.h"

unsigned char scr_cols;
unsigned char scr_wide;
unsigned char scr_color;

static unsigned char far *video;
static unsigned char entry_mode;        /* restored by plat_shutdown()    */
static unsigned char snow;              /* /SNOW: retrace-gate the writes */
static unsigned char want40, want80, wantmono;

/*
 * The three faces of one interface. Colour is the EDIT.EXE family's look --
 * white on blue desktop, grey bars -- with the hint bar pulled to Gmail red
 * the way the Atari's palette does. Black-and-white keeps the same shapes in
 * normal, bright and reverse. The MDA table is where mode 7 earns its own
 * column: bright for unread, reverse for the bars, and a real underline for
 * the reader's subject, which is the one attribute no other adapter has.
 */
static const unsigned char attrs_color[N_ATTRS] = {
    0x17,   /* A_TEXT   white on blue                */
    0x1F,   /* A_EMPH   bright white on blue         */
    0x70,   /* A_SEL    black on light grey          */
    0x70,   /* A_BAR    black on light grey          */
    0x4F,   /* A_FOOT   bright white on red          */
    0x1F,   /* A_UNDER  bright white on blue         */
};

static const unsigned char attrs_bw[N_ATTRS] = {
    0x07, 0x0F, 0x70, 0x70, 0x70, 0x0F,
};

static const unsigned char attrs_mda[N_ATTRS] = {
    0x07, 0x0F, 0x70, 0x70, 0x70,
    0x01,   /* A_UNDER  underline, the MDA's own     */
};

static const unsigned char *scr_attr = attrs_bw;

unsigned char scr_attr_byte(unsigned char attr)
{
    return scr_attr[attr];
}

/* ------------------------------------------------------------------ */
/* BIOS                                                                */
/* ------------------------------------------------------------------ */

static unsigned char cur_mode(unsigned char *cols)
{
    union REGS r;

    r.h.ah = 0x0F;
    int86(0x10, &r, &r);

    if (cols)
        *cols = r.h.ah;
    return r.h.al;
}

static void set_mode(unsigned char mode)
{
    union REGS r;

    r.h.ah = 0x00;
    r.h.al = mode;
    int86(0x10, &r, &r);
}

static void hide_cursor(void)
{
    union REGS r;

    r.h.ah = 0x01;
    r.x.cx = 0x2000;            /* start line bit 5: cursor off */
    int86(0x10, &r, &r);

    /* Some BIOSes show it anyway; parking it below the screen covers them. */
    r.h.ah = 0x02;
    r.h.bh = 0x00;
    r.x.dx = 0x1900;
    int86(0x10, &r, &r);
}

/* ------------------------------------------------------------------ */
/* Command tail                                                        */
/* ------------------------------------------------------------------ */

static void parse_tail(void)
{
    const char far *t = MK_FP(_psp, 0x81);
    unsigned char   n = *(const unsigned char far *) MK_FP(_psp, 0x80);
    unsigned char   c;

    while (n--) {
        if (*t++ != '/')
            continue;
        if (n == 0)
            break;
        c = (unsigned char) *t;
        if (c >= 'a' && c <= 'z')
            c -= 0x20;

        if (c == '4')
            want40 = 1;
        else if (c == '8')
            want80 = 1;
        else if (c == 'M')
            wantmono = 1;
        else if (c == 'S')
            snow = 1;
    }
}

/* ------------------------------------------------------------------ */
/* Blitter                                                             */
/* ------------------------------------------------------------------ */

/*
 * One cell. The /SNOW wait spins to the *start* of a horizontal retrace so
 * the whole word lands inside it; waiting merely for "retrace active" can
 * catch its final cycles and write the char during display anyway.
 */
static void put(unsigned char far *p, unsigned char g, unsigned char a)
{
    if (snow) {
        while (inp(0x3DA) & 1)
            ;
        while (!(inp(0x3DA) & 1))
            ;
    }
    p[0] = g;
    p[1] = a;
}

static unsigned char far *cellp(unsigned char row, unsigned char col)
{
    return video + ((unsigned int) row * scr_cols + col) * 2;
}

void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr)
{
    unsigned char far *p = cellp(row, col);
    unsigned char      a = scr_attr[attr];
    unsigned char      n = 0;

    while (n < width && *s) {
        put(p, (unsigned char) *s++, a);
        p += 2;
        n++;
    }
    while (n < width) {
        put(p, ' ', a);
        p += 2;
        n++;
    }
}

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char attr)
{
    unsigned char far *p = cellp(row, col);
    unsigned char      a = scr_attr[attr];

    while (*s) {
        put(p, (unsigned char) *s++, a);
        p += 2;
    }
}

void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char attr)
{
    unsigned char len = 0;
    const char   *q = s;

    while (*q++)
        len++;
    if (len > rcol + 1)
        len = rcol + 1;

    scr_text(row, (unsigned char) (rcol + 1 - len), s, attr);
}

void scr_center(unsigned char row, const char *s, unsigned char attr)
{
    unsigned char len = 0;
    const char   *q = s;

    while (*q++)
        len++;
    if (len > scr_cols)
        len = scr_cols;

    scr_text(row, (unsigned char) ((scr_cols - len) / 2), s, attr);
}

void scr_fill(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char width, unsigned char attr)
{
    unsigned char far *p = cellp(row, col);
    unsigned char      a = scr_attr[attr];

    while (width--) {
        put(p, glyph, a);
        p += 2;
    }
}

void scr_cell(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char rawattr)
{
    put(cellp(row, col), glyph, rawattr);
}

void scr_row_clear(unsigned char row)
{
    scr_field(row, 0, "", scr_cols, A_TEXT);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    while (first <= last)
        scr_row_clear(first++);
}

void scr_clear(void)
{
    unsigned char row;

    for (row = 0; row < SCR_ROWS; row++)
        scr_row_clear(row);
}

#ifdef GM_SHOT
/*
 * The capture the per-platform tools/*-shot.sh scripts all need somewhere.
 * The Atari and CoCo reach into a paused emulator for theirs; a DOS program
 * can simply hand its own text page over -- the file lands on the mounted
 * host directory, and the B800 pair format is its own documentation.
 *
 * The header carries the BIOS mode the probe saw, because the mode decides
 * everything else -- which attribute table is in force and which segment
 * this page was read from -- and a capture that cannot say "that was really
 * mode 7" cannot check the MDA path at all.
 */
#include <stdio.h>

void scr_snapshot(void)
{
    FILE *f = fopen("SCREEN.BIN", "wb");
    unsigned int i;
    unsigned int n = (unsigned int) scr_cols * SCR_ROWS * 2;

    if (!f)
        return;
    fputc(scr_cols, f);
    fputc(SCR_ROWS, f);
    fputc(cur_mode(0), f);
    for (i = 0; i < n; i++)
        fputc(video[i], f);
    fclose(f);
}
#endif

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void plat_init(void)
{
    unsigned char mode, cols, want;
    unsigned char mono_hw;

    parse_tail();

    mode = cur_mode(&cols);
    entry_mode = mode;

    /*
     * The adapter, not the mode, decides where the text page is. Bits 4-5 of
     * the BDA equipment word are 11 for a monochrome adapter, and that is
     * the authoritative test: an MDA or Hercules machine is not necessarily
     * *in* mode 7 when this program starts -- dosbox-x's hercules machine
     * boots reporting mode 3 -- but its page is at B000 regardless, and a
     * probe that trusted the mode wrote 4,000 bytes into an address no
     * hardware was decoding. The same reasoning, and the same word, as
     * fujinet-config's screen_get_video_segment_address().
     */
    mono_hw = ((*(unsigned short far *) MK_FP(0x0040, 0x0010) & 0x30)
               == 0x30);

    if (mono_hw) {
        /* One adapter, one mode, one width: normalise to 7 and ignore the
           width switches rather than ask for hardware that is not there. */
        if (mode != 7) {
            set_mode(7);
            mode = cur_mode(&cols);
        }
    } else {
        want = mode;
        if (want40)
            want = wantmono ? 0 : 1;
        else if (want80)
            want = wantmono ? 2 : 3;
        else if (mode > 3 || cols < 40)
            want = 3;

        if (want != mode || cols > 80) {
            set_mode(want);
            mode = cur_mode(&cols);
        }
    }

    video = MK_FP((mono_hw || mode == 7) ? 0xB000 : 0xB800, 0x0000);

    if (mono_hw || mode == 7)
        scr_attr = attrs_mda;
    else if ((mode == 1 || mode == 3) && !wantmono)
        scr_attr = attrs_color;
    else
        scr_attr = attrs_bw;
    scr_color = (scr_attr == attrs_color);

    scr_cols = (cols >= 80) ? 80 : 40;
    scr_wide = (scr_cols >= 80);

    /* The runtime half of the GM_RT_COLS hook: body text is painted at
       column 1 in a field two short of the width, so that is the wrap. */
    gm_wrap_cols = (unsigned char) (scr_cols - 2);

    hide_cursor();
    scr_clear();

    ui_geom();
}

void plat_shutdown(void)
{
    /* Setting the mode -- even the same one -- clears the screen and brings
       the cursor back, which is the whole restoration. */
    set_mode(entry_mode);
}

/*
 * Nothing to bracket. An INT F5 call is a blocking subroutine into the
 * resident driver; no interrupt of ours runs during it and no display
 * hardware needs quieting -- the Apple backend's situation exactly, and the
 * empty pair is kept for the same reason: they are correct from the same
 * source on the Atari.
 */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
