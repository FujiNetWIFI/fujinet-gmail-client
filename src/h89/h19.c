/**
 * @file h19.c
 * @brief H19 terminal screen - Heathkit H89.
 * @author Thomas Cherryhomes
 * @email thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 *
 * From fujinet-config's src/h89 (add-h89, 92cca0e), by way of the
 * Google Calendar client's H89 port (add-h89, c46d930); see h19.h for what
 * changed.
 *
 * Two copies of the screen: want, which the screens draw into, and have,
 * what the terminal shows. h19_flush() sends the difference. A whole screen
 * is about 2K - two seconds at 9600 baud - but the chrome is the same on
 * every view, so moving between them costs only what really changes.
 */

#include <string.h>
#include "con.h"
#include "h19.h"
#include "kbd.h"

#define ESC 0x1B
#define UNKNOWN 0xFF
#define STALE_FOR_ERASE 4

static uint8_t want[H19_COLS * H19_ROWS];
static uint8_t have[H19_COLS * H19_ROWS];
static uint8_t dirty[H19_ROWS];

static uint8_t cursor_x = H19_NONE, cursor_y;

/* What the terminal is doing now. UNKNOWN makes the next cell set it. */
static uint8_t cur_x, cur_y, cur_rev, cur_gfx, cur_shown;
static uint8_t active;

static void esc(uint8_t c)
{
  con_out(ESC);
  con_out(c);
}

static void forget(void)
{
  cur_x = cur_y = cur_rev = cur_gfx = UNKNOWN;
}

static void rev_off(void)
{
  if (cur_rev)
  {
    cur_rev = 0;
    esc('q');
  }
}

void h19_init(void)
{
  con_init();

  esc('G');      /* graphics mode off */
  esc('q');      /* reverse video off */
  esc('x');      /* cursor off */
  con_out('5');
  esc('x');      /* a block cursor, for when the form shows it */
  con_out('4');
  esc('w');      /* discard at the end of a line: never wrap or scroll */
  esc('t');      /* keypad shifted: the unshifted keypad sends ESC A-D */
  esc('x');      /* the 25th line on */
  con_out('1');
  esc('E');      /* clear lines 1-24 */
  esc('Y');      /* and the 25th */
  con_out(32 + H19_STATUS_ROW);
  con_out(32);
  esc('l');

  memset(want, ' ', sizeof(want));
  memset(have, ' ', sizeof(have));
  memset(dirty, 0, sizeof(dirty));
  forget();
  cur_rev = cur_gfx = cur_shown = 0;
  cursor_x = H19_NONE;
  active = 1;
}

void h19_exit(void)
{
  static uint16_t t;

  if (!active)
    return;
  active = 0;

  /* Back to the power-up configuration the DIP switches set - modes,
     cursor, keypad, the 25th line off - and a clear screen. The terminal
     needs a moment for it. */
  esc('y');
  con_out('1');
  esc('z');
  t = TICKS;
  while ((uint16_t)(TICKS - t) < 100)
    ;
  con_exit();
}

void h19_begin(void)
{
  memset(want, ' ', H19_COLS * H19_STATUS_ROW);
  memset(dirty, 1, H19_STATUS_ROW);
  cursor_x = H19_NONE;
}

uint8_t *h19_line(uint8_t y)
{
  dirty[y] = 1;
  return &want[(uint16_t)y * H19_COLS];
}

void h19_cursor(uint8_t x, uint8_t y)
{
  cursor_x = x;
  cursor_y = y;
}

void h19_bell(void)
{
  con_out(0x07);
}

/* The reader's alone, so under HDOS it rides in the reader's overlay. */
#ifdef GM_OVERLAYS
#pragma codeseg code_ovl_browse
#endif

static void go_line(uint8_t y)
{
  esc('Y');
  con_out(32 + y);
  con_out(32);
}

/*
 * Delete line at one end, insert line at the other. The lines under the
 * region are pushed out by the one and back by the other, so they end where
 * they started; the 25th line is not part of the page and does not move.
 */
void h19_scroll(uint8_t y0, uint8_t y1, int8_t dir)
{
  static uint8_t del, ins;
  static uint16_t span;

  if (!active || y0 >= y1 || y1 >= H19_STATUS_ROW)
    return;

  if (dir > 0)
  {
    del = y0;
    ins = y1;
  }
  else
  {
    del = y1;
    ins = y0;
  }

  /* An inserted line is blank whatever mode the terminal is in, but say so
     anyway rather than find out */
  rev_off();
  go_line(del);
  esc('M');
  /* Scrolling up into the last line: the delete already blanked it */
  if (!(dir > 0 && ins == H19_STATUS_ROW - 1))
  {
    go_line(ins);
    esc('L');
  }
  cur_x = UNKNOWN;

  span = (uint16_t)(y1 - y0) * H19_COLS;
  if (dir > 0)
  {
    memmove(&have[(uint16_t)y0 * H19_COLS], &have[(uint16_t)(y0 + 1) * H19_COLS], span);
    memset(&have[(uint16_t)y1 * H19_COLS], ' ', H19_COLS);
  }
  else
  {
    memmove(&have[(uint16_t)(y0 + 1) * H19_COLS], &have[(uint16_t)y0 * H19_COLS], span);
    memset(&have[(uint16_t)y0 * H19_COLS], ' ', H19_COLS);
  }
  memset(&dirty[y0], 1, y1 - y0 + 1);
}

#ifdef GM_OVERLAYS
#pragma codeseg code_compiler
#endif

/** 1 when the cell needs graphics mode, 0 when it needs it off, 2 when it
    looks the same either way */
static uint8_t gfx_of(uint8_t code)
{
  code &= 0x7F;
  if (code < 0x20 || code == 0x7F)
    return 1;
  if (code >= 0x5E)
    return 0;
  return 2;
}

/** Send one cell where the cursor is */
static void send(uint8_t code)
{
  static uint8_t g, m;

  if ((code & REV) != cur_rev)
  {
    cur_rev = code & REV;
    esc(cur_rev ? 'p' : 'q');
  }

  m = gfx_of(code);
  if (m != 2 && m != cur_gfx)
  {
    cur_gfx = m;
    esc(m ? 'F' : 'G');
  }

  g = code & 0x7F;
  if (g == 0x7F)
    g = '^';
  else if (g == 0x1F)
    g = '_';
  else if (g < 0x20)
    g += 0x60;
  con_out(g);

  /* With discard at the end of a line the cursor stays on the last column */
  cur_x = (cur_x == H19_COLS - 1) ? UNKNOWN : cur_x + 1;
}

/** Move the cursor to x on line y. A short hop along the line resends the
    cells in between when that is no dearer than ESC Y and keeps the modes. */
static void go(uint8_t x, uint8_t y, const uint8_t *row)
{
  static uint8_t i, c, m;

  if (cur_y == y && cur_x != UNKNOWN && cur_x < x && x - cur_x <= 3)
  {
    for (i = cur_x; i < x; i++)
    {
      c = row[i];
      m = gfx_of(c);
      if ((c & REV) != cur_rev || (m != 2 && m != cur_gfx))
        break;
    }
    if (i == x)
    {
      for (i = cur_x; i < x; i++)
        send(row[i]);
      return;
    }
  }

  if (cur_x != x || cur_y != y)
  {
    esc('Y');
    con_out(32 + y);
    con_out(32 + x);
    cur_x = x;
    cur_y = y;
  }
}

static void flush_row(uint8_t y)
{
  static uint8_t *row, *hrow;
  static uint8_t x, e, last, stale, i;

  row = &want[(uint16_t)y * H19_COLS];
  hrow = &have[(uint16_t)y * H19_COLS];

  /* The last column before the right edge that is not blank: past it, a
     line with enough old text left on it is cleared with one ESC K */
  last = UNKNOWN;
  for (x = 0; x < H19_COLS - 1; x++)
    if (row[x] != ' ')
      last = x;

  for (x = 0; x < H19_COLS; x++)
  {
    e = row[x];
    if (hrow[x] == e)
      continue;

    if (e == ' ' && (last == UNKNOWN || x > last) && x < H19_COLS - 1)
    {
      stale = 0;
      for (i = x; i < H19_COLS - 1; i++)
        if (hrow[i] != ' ')
          stale++;
      if (stale >= STALE_FOR_ERASE)
      {
        go(x, y, row);
        rev_off();
        esc('K');
        memset(&hrow[x], ' ', H19_COLS - x);
        continue;
      }
    }

    go(x, y, row);
    send(e);
    hrow[x] = e;
  }

  dirty[y] = 0;
}

static void flush(uint8_t yield)
{
  static uint8_t y;

  if (!active)
    return;

  for (y = 0; y < H19_ROWS; y++)
    if (dirty[y])
    {
      if (yield && kbd_pending())
        return;
      flush_row(y);
    }

  if (cursor_x != H19_NONE)
  {
    if (cur_x != cursor_x || cur_y != cursor_y)
    {
      esc('Y');
      con_out(32 + cursor_y);
      con_out(32 + cursor_x);
      cur_x = cursor_x;
      cur_y = cursor_y;
    }
    if (!cur_shown)
    {
      esc('y');
      con_out('5');
      cur_shown = 1;
    }
  }
  else if (cur_shown)
  {
    esc('x');
    con_out('5');
    cur_shown = 0;
  }
}

void h19_flush(void)
{
  flush(0);
}

void h19_flush_poll(void)
{
  flush(1);
}
