/*
 * FujiNet Gmail client -- shared types and constants.
 *
 * Ported from the IntyBASIC original in intv/. Everything in src/ is meant to
 * stay portable across MekkoGX platforms; anything that touches a specific
 * machine lives in src/<platform>/ behind the ui_* / plat_* interface below.
 */

#ifndef GMAIL_H
#define GMAIL_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Sizing                                                              */
/* ------------------------------------------------------------------ */

/*
 * Entries fetched per range. Each one costs the adapter an extra upstream HTTPS
 * round trip inside the open(), so this is deliberately modest.
 *
 * It is also the inbox's list height, because no backend scrolls a window
 * inside a page -- ui_inbox() paints gm_count rows and that is the whole list.
 * A machine with fewer rows than this has to lower it rather than invent a
 * second number: the CoCo's sixteen-row screen has eleven to spend.
 */
#ifndef IDX_MAX
#define IDX_MAX         16
#endif

/*
 * Wrapped body rows kept before we give up and set the truncation flag, and
 * the width they are wrapped to.
 *
 * A wider screen wants fewer rows, not more: the same message needs about half
 * as many at 78 columns as at 40. Check the platform's map file before raising
 * either -- the Atari has about 700 bytes of BSS to its name and every one of
 * these numbers is spent against the same ceiling.
 *
 * 264, down from the 300 this started at, is what pays for a compose form that
 * holds more than a screenful: FRM_NBODY 12 -> 48 costs 1,404 bytes and 36
 * body rows give back 1,476. The reader loses two pages of a very long message
 * (264 x 40 is ~10,500 characters, still fourteen and a half screens) and the
 * form gains three and a half screens to type into, which is the better half
 * of that trade for a mail client that can now reply properly.
 */
#ifndef BODY_ROWS
#define BODY_ROWS       224
#endif
#ifndef BODY_COLS
#define BODY_COLS       40
#endif

/* Derived on purpose, and deliberately not overridable. A backend that could
   set the stride independently of the width would eventually set one and not
   the other, and wrap_text() would write BODY_COLS+1 bytes into a shorter row
   with nothing to catch it. */
#define BODY_STRIDE     (BODY_COLS + 1)

/*
 * The wrap width body.c actually uses. Equal to BODY_COLS everywhere except a
 * backend that cannot know its screen width until it boots -- MS-DOS inherits
 * whatever video mode it is started in, 40 columns or 80. Such a backend
 * builds with -DGM_RT_COLS, sizes storage for its widest case through
 * BODY_COLS above, and sets gm_wrap_cols in plat_init(), never above
 * BODY_COLS. Wrapping narrower than the stride is safe; the reverse is the
 * overflow the BODY_STRIDE comment describes, which is why the stride stays
 * derived and only the width gets a runtime form.
 */
#ifdef GM_RT_COLS
extern unsigned char gm_wrap_cols;
#define WRAP_COLS   gm_wrap_cols
#else
#define WRAP_COLS   BODY_COLS
#endif

/* Longest raw line accumulated before a hard flush through the wrapper. It has
   to exceed BODY_COLS by enough that flush_overflow() has a space to break on;
   a couple of display rows' worth is plenty. */
#ifndef LINE_CAP
#define LINE_CAP        200
#endif

#if LINE_CAP < BODY_COLS * 2
#error "LINE_CAP must hold at least two display rows"
#endif

#define ENT_NUM_LEN     12      /* msgNum as ASCII decimal, NUL terminated */
#define ENT_NAME_LEN    32      /* the whole wire field; never needs raising */

/* Truncated from the wire's 128 to fit the Atari's budget: 16 entries deep,
   every byte here costs sixteen. A backend with room asks for more. */
#ifndef ENT_SUBJ_LEN
#define ENT_SUBJ_LEN    64
#endif

/* Rendered date column, "Aug 28 14:32" or "Aug 28  2024". */
#define ENT_DATE_LEN    13

/* ------------------------------------------------------------------ */
/* Wire format                                                         */
/* ------------------------------------------------------------------ */

/*
 * Mailbox.h's MailIndexItem, #pragma pack(1), 220 bytes. The firmware carries
 * a static_assert(sizeof(MailIndexItem) == 220) as a tripwire against this
 * layout changing -- if a firmware update trips it, this struct is what moves.
 *
 * A record is read straight into this, which works because no compiler in this
 * family inserts struct padding: cc65 has none to insert on a 6502, and CMOC
 * none on a 6809.
 *
 * Every multi-byte number is bytes, not an integer, and that is not fussiness.
 * The wire is little-endian and so is the 6502, so msgnum used to be a uint32_t
 * -- and then the 6809 arrived, which is big-endian, and read every message
 * number and the whole folder size backwards. Nothing on screen would have
 * looked obviously wrong; the numbers go straight back out in a URL, so every
 * open would simply have 404'd. rd32le() in net.c is now the only thing that
 * knows the wire's byte order, the same way date.c is for ts[8].
 */
struct wire_rec {
    uint8_t  msgnum[4];         /*   0    4  little-endian u32 */
    char     name[32];          /*   4   32  NUL-terminated / NUL-padded */
    char     email[48];         /*  36   48  fallback when name[0] == 0 */
    char     subject[128];      /*  84  128  */
    uint8_t  ts[8];             /* 212    8  little-endian u64, epoch seconds */
};

#define REC_STRIDE  220

/* Access modes (Protocol.h ACCESS_MODE). On a DIRECTORY open aux2 is a format
   selector: 255 = raw structs. On a READ open aux2 is the real EOL
   translation code. On a WRITE open aux2 is unused and stays 0. */
#define MB_MODE_READ    4
#define MB_MODE_DIR     6
#define MB_MODE_WRITE   8
#define MB_FMT_RAW      255
#define MB_TRANS_CRLF   3

/* The adapter buffers the whole draft and refuses to grow past this
   (MB_MAX_WRITE in the firmware's Mailbox.cpp). Tripping the device-side cap
   poisons the draft -- 162 on the write and the close cannot commit -- so the
   client stops strictly short of it; see the budget check in form.c. */
#define GM_SEND_MAX     16384

/* NDEV_STATUS codes (status_error_codes.h) that this client maps.
   Success is reported as 0 over SIO and as 1 elsewhere -- see st_ok() in
   net.c. GM_NOREPLY is ours, not the device's: it means the status call
   itself failed, which is distinct from the device answering "0 bytes". */
#define GM_OK           1
#define GM_NOREPLY      0xFF
#define GM_EOF          136     /* buffer drained -- normal, not an error */
#define GM_REJECTED     132     /* draft refused: bad header line / no recipient */
#define GM_TOOBIG       162     /* draft exceeded the adapter's write cap */
#define GM_DENIED       167
#define GM_NOTFOUND     170
#define GM_NOSERVICE    210
#define GM_NOAUTH       212

/*
 * fn_default_timeout is in 64-frame ticks. 15 (the library default) is ~16s;
 * a GMAIL index open is ~19 sequential upstream HTTPS round trips.
 *
 * SIO only. fujinet-lib reads this in exactly one place, the Atari bus layer's
 * copy_cmd_data.s, where it becomes DCB's DTIMLO. On SmartPort there is no
 * host-side timeout at all -- the call is into card firmware that blocks until
 * the FujiNet answers -- so the assignments in net.c compile, link and do
 * nothing there. They stay because they are correct from the same source on the
 * Atari.
 */
#define TMO_NORM        15
#define TMO_LONG        90      /* ~96 seconds */

/* ------------------------------------------------------------------ */
/* Parsed model                                                        */
/* ------------------------------------------------------------------ */

struct entry {
    /* The message number goes straight back out in a URL and is never used
       for arithmetic, so it is kept as the ASCII decimal we rendered once. */
    char    num[ENT_NUM_LEN];
    char    name[ENT_NAME_LEN];
    char    subject[ENT_SUBJ_LEN];
    uint8_t ts[8];              /* raw little-endian wire bytes */
    uint8_t unread;
};

extern struct entry  gm_index[IDX_MAX];
extern unsigned char gm_count;      /* entries actually parsed, 0..IDX_MAX */
extern unsigned char gm_sel;        /* selected slot, 0..gm_count-1 */
extern unsigned long gm_range;      /* absolute index of gm_index[0] */
extern unsigned long gm_total;      /* total messages in the folder */
extern unsigned char gm_next;       /* is there a page after this one */
extern unsigned char gm_list_valid; /* a usable listing is on screen */

extern char          gm_body[BODY_ROWS][BODY_STRIDE];
extern unsigned int  gm_body_rows;
extern unsigned char gm_body_trunc;

extern unsigned char gm_ecode;      /* last error code for the error screen */
extern unsigned char gm_dev_ecode;  /* raw device code behind it, for diagnosis */
extern const char   *gm_stage;      /* which step failed: open / status / read */

/* ------------------------------------------------------------------ */
/* Compose form                                                        */
/* ------------------------------------------------------------------ */

/*
 * Field capacities, excluding the NUL. The body is FRM_NBODY separate line
 * fields on the same form engine as TO and SUBJECT. Enter moves to the next
 * line, and a line that fills pushes its trailing word down to the one below
 * -- see form_body_spill(). All three knobs are per-platform RAM and screen
 * decisions and come from the Makefile.
 *
 * FRM_NBODY is what the form *stores*; FRM_VBODY is how many of those lines
 * are on screen at once, and compose.c scrolls the window between them. They
 * were one number while the form could hold only a screenful, which is
 * exactly what made it hold only a screenful. Keeping them apart is what
 * lets the storage grow without asking any backend to find more rows: a
 * backend lays out FRM_VBODY rows and never sees FRM_NBODY at all.
 */
#ifndef FRM_NBODY
#define FRM_NBODY       48      /* body lines the form holds */
#endif
#ifndef FRM_VBODY
#define FRM_VBODY       12      /* body lines visible at once */
#endif
#ifndef FRM_BODY_COLS
#define FRM_BODY_COLS   38      /* storage width of one body line */
#endif
#define FRM_TO_MAX      63
#define FRM_SUBJ_MAX    63

#if FRM_VBODY > FRM_NBODY
#error "FRM_VBODY must not exceed FRM_NBODY"
#endif

/* The emit/echo scratch must hold the longest emitted line -- a header, the
   forward block's full-width original subject, or a body row (the form's
   own or a forwarded gm_body one) -- plus the terminator, with the widest
   echo window always narrower. */
#define FRM_LMAX1   ((ENT_SUBJ_LEN > FRM_SUBJ_MAX) ? ENT_SUBJ_LEN : FRM_SUBJ_MAX)
#define FRM_LMAX2   ((FRM_LMAX1 > FRM_BODY_COLS) ? FRM_LMAX1 : FRM_BODY_COLS)
#define FRM_LMAX3   ((FRM_LMAX2 > BODY_COLS) ? FRM_LMAX2 : BODY_COLS)
#define FRM_LINE_MAX (FRM_LMAX3 + 16)

/* Field indices. Body lines are ordinary fields from F_BODY0 up. */
#define F_TO        0
#define F_SUBJ      1
#define F_BODY0     2
#define FRM_NFIELDS (2 + FRM_NBODY)

/* Form modes. Reply and forward act on gm_index[gm_sel]. */
#define FRM_COMPOSE 0
#define FRM_REPLY   1
#define FRM_FWD     2

/*
 * The form's storage: nothing but chars, so there is no padding to make
 * form_field_ptr()'s arithmetic and the members drift. Deliberately plain
 * BSS with no overlay trick: a forward reads gm_index[gm_sel] and every row
 * of gm_body at emit time, so the form cannot borrow the body buffer the
 * way the calendar client's does.
 */
struct frmbuf {
    char to[FRM_TO_MAX + 1];
    char subj[FRM_SUBJ_MAX + 1];
    char body[FRM_NBODY][FRM_BODY_COLS + 1];
    char line[FRM_LINE_MAX];    /* emit and echo scratch, shared */
};

extern struct frmbuf  frm;
extern unsigned char  frm_dirty[FRM_NFIELDS];
extern unsigned char  frm_mode;     /* FRM_*, set by form_init() */

/* Form messages, drawn by ui_form_msg(). FM_NONE restores the normal hints. */
#define FM_NONE     0
#define FM_ASK      1           /* send? yes / no */
#define FM_NEEDTO   2
#define FM_NEEDBODY 3

/* form.c -- the form model. Pure; tests/hosttest.c exercises all of it.
   form_emit sends the draft one line at a time through gm_send_put() --
   net.c's in the real program, the capture in the tests. Write failures are
   gm_send_put's own to latch, and gm_send_end() is where they report. */
void          form_init(unsigned char mode);
char         *form_field_ptr(unsigned char f);
unsigned char form_field_max(unsigned char f);
unsigned char form_any_dirty(void);
unsigned char form_validate(unsigned char *bad);
void          form_emit(void);

/* Automatic wrap: push the trailing word of body line *line down onto the
   line below, moving the cursor (*line, *pos) with it when the cursor was in
   that word. Returns 1 when room was made. One line deep and forward only --
   see the note on the definition. */
unsigned char form_body_spill(unsigned char *line, unsigned char *pos);

/* compose.c -- the form screen. Reply and forward act on gm_index[gm_sel];
   the caller repaints its own screen afterwards either way. */
void          compose_new(void);
void          compose_reply(void);
void          compose_forward(void);

/* ------------------------------------------------------------------ */
/* Portable services                                                   */
/* ------------------------------------------------------------------ */

/* sanitize.c -- copy a NUL-terminated wire field into a fixed-width buffer,
   clamping control bytes to space and collapsing each run of bytes > 126 to a
   single '?'. Always NUL-terminates. This is the entire charset story: no
   RFC 2047 decoding, no UTF-8. It also guarantees no stray EOL byte reaches
   the screen driver. */
void copy_san(char *dst, const char *src, unsigned char dstsize);

/* wrap.c -- greedy whole-word wrap into a row array of `stride` bytes each.
   Words longer than `cols` are hard-split. On row-budget overflow the last
   row is ellipsized with "...". Returns the number of rows produced. */
unsigned int wrap_text(const char *src, char *rows, unsigned int max_rows,
                       unsigned char cols, unsigned char stride);

/* body.c -- folds the incoming byte stream into wrapped display rows. */
void          body_reset(void);
void          body_ingest(const unsigned char *p, unsigned int n);
void          body_finish(void);

/* net.c -- both return 1 on success, 0 on failure with gm_ecode set. */
unsigned char gm_fetch_index(unsigned long range);
unsigned char gm_fetch_body(const char *msgnum);
void          gm_calc_next(void);

/* net.c -- the draft channel. begin opens it (a reply open is where the
   target is resolved), gm_send_put() writes the draft lines into it, end
   closes -- which is what commits -- and reads the verdict, a latched write
   failure included. gm_send_room() is what is left of the adapter's cap. */
unsigned char gm_send_begin(unsigned char reply, const char *msgnum);
void          gm_send_put(const char *line);
unsigned int  gm_send_room(void);
unsigned char gm_send_end(void);

/* date.c -- render a wire timestamp into ENT_DATE_LEN bytes as "Aug 28 14:32",
   or "Aug 28  2024" for a message outside gm_year. Pure: no platform, no
   network, no device call. tests/hosttest.c exercises all of it. */
void          date_fmt(char *dst, const uint8_t ts[8]);

/* clock.c -- what date.c needs from the device, plus the wall clock's starting
   point. gm_tzoff is minutes east of UTC and gm_year the current year, or 0 for
   "unknown", which is also what they stay at if the FujiNet has no clock
   registered: dates then read as UTC and always carry a time. */
extern int          gm_tzoff;
extern unsigned int gm_year;
void          clock_load(void);

/* tick.c -- the wall clock between device reads. gm_clock_ok is 0 when the
   device never answered, and nothing draws a clock then. tick_reset() is
   clock.c's to call after a good read; tick_advance() and tick_due_resync()
   belong to main.c's loops; clock_pump() is what every backend's blocking key
   wait calls once round, and is the only one that paints. */
extern unsigned char gm_h, gm_mi, gm_s;
extern unsigned char gm_clock_ok;

void          tick_reset(void);
unsigned char tick_advance(void);
unsigned char tick_due_resync(void);
void          clock_pump(void);

/* hwm.c -- read/unread high-water mark, persisted in a FujiNet appkey. */
void          hwm_load(void);
void          hwm_flags(void);                      /* recompute all unread bits */
void          hwm_update(const uint8_t *ts);        /* advance + persist if newer */

/* ------------------------------------------------------------------ */
/* Platform backend -- implemented per target under src/<platform>/     */
/* ------------------------------------------------------------------ */

/* Visible body rows in the message reader. The inbox's list height is IDX_MAX
   -- see the comment there -- and does not get a constant of its own. */
#ifndef MSG_ROWS
#define MSG_ROWS    18
#endif

/* Portable key codes returned by plat_getkey(). R returns K_REPLY from every
   backend; the inbox loop folds it into K_REFRESH, so R still refreshes
   there and only the reader treats it as reply. */
#define K_NONE      0
#define K_UP        1
#define K_DOWN      2
#define K_LEFT      3
#define K_RIGHT     4
#define K_ENTER     5
#define K_BACK      6
#define K_REFRESH   7
#define K_QUIT      8
#define K_COMPOSE   9           /* C, inbox */
#define K_REPLY     10          /* R, reader */
#define K_FORWARD   11          /* F, reader */

/*
 * The form's key read. Printable ASCII $20-$7E passes through verbatim;
 * everything with a meaning maps to an E_* code below $20, so the two can
 * never collide. Blocks like plat_getkey(). The E_* values deliberately
 * coincide with K_* 1-8 so one scripted fake-key stream can drive both
 * loops.
 *
 * Not every backend can produce every code: a keyboard without cursor keys
 * simply never sends E_LEFT, and the editor edits append-and-backspace there.
 */
#define E_ENTER     1           /* next field */
#define E_UP        2
#define E_DOWN      3
#define E_LEFT      4
#define E_RIGHT     5
#define E_BS        6           /* delete before the cursor */
#define E_DONE      7           /* leave the form (ESC / BREAK / SmartKey) */
#define E_SAVE      8           /* send now, skipping the ask (Adam SmartKey) */

unsigned char plat_getch(void);

void          plat_init(void);
void          plat_shutdown(void);

/* Bracket every network and fuji device call. On the Atari this suppresses
   display list interrupts, which would otherwise steal cycles from a
   timing-critical SIO transfer. */
void          plat_net_begin(void);
void          plat_net_end(void);

unsigned char plat_getkey(void);        /* blocks, returns a K_* code */
void          plat_anykey(void);        /* blocks until any key */

/*
 * Frame timing, and the obligation that comes with it.
 *
 * plat_ticks() is a free-running counter in plat_fps() units. It has to go on
 * rising while a screen sits waiting for a key, because that is the whole of
 * what the wall clock runs on -- which is why every blocking wait above is a
 * poll around plat_vsync() and clock_pump() rather than a firmware call that
 * never comes back. On the machines whose counter is the OS's own (the Atari's
 * RTCLOK, the PC's BIOS tick) that is belt and braces; on the ones this program
 * counts itself it is the only thing keeping the clock honest.
 */
void          plat_vsync(void);
unsigned long plat_ticks(void);
unsigned char plat_fps(void);

/* Busy overlay reasons. */
#define BUSY_INDEX  1
#define BUSY_BODY   2
#define BUSY_SEND   3

void          ui_splash(void);
void          ui_notfound(void);
void          ui_busy(unsigned char reason);
void          ui_error(unsigned char code);
void          ui_inbox(void);                   /* full repaint */
void          ui_inbox_sel(unsigned char from, unsigned char to);
void          ui_message(unsigned int top);     /* header + body window */
void          ui_sent(void);                    /* flat "message sent" screen */

/*
 * The wall clock, in whatever slot the screen that is up has for it -- which
 * is the backend's business, and is nothing at all on the flat screens, where
 * the program is inside a device call and the clock is stopped anyway. Painted
 * by the screen painters above on a repaint, and by clock_pump() once a minute
 * in between. Draws nothing while gm_clock_ok is 0.
 */
void          ui_clock(void);

/*
 * The form screen. ui_form() paints the chrome -- title, field labels, the
 * footer hints -- and nothing inside the fields; compose.c then draws every
 * row through ui_form_row(), which is also how each keystroke is echoed.
 *
 * ui_form_row() gets the visible slice of the field already windowed --
 * compose.c owns the horizontal scroll -- with curx the cursor's column
 * within it, only meaningful while `active`. This is deliberately the one
 * hook where the backends' inv-flag / attribute-role split lives.
 *
 * ui_form_width() reports how many text columns field f's window has, so the
 * engine and the painter cannot disagree about where the scroll lands.
 */
void          ui_form(unsigned char mode);
unsigned char ui_form_width(unsigned char f);
void          ui_form_row(unsigned char f, const char *win,
                          unsigned char curx, unsigned char active);
void          ui_form_msg(unsigned char msg);

#endif /* GMAIL_H */
