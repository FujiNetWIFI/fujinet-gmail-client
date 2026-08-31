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
 * as many at 78 columns as at 40. The Atari's 300 x 40 is ~12000 characters and
 * leaves ~8K of its BSS budget free; the Apple II's 240 x 78 is ~18700 in about
 * 6.7K more. Check the platform's map file before raising either.
 */
#ifndef BODY_ROWS
#define BODY_ROWS       300
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
   translation code. */
#define MB_MODE_READ    4
#define MB_MODE_DIR     6
#define MB_FMT_RAW      255
#define MB_TRANS_CRLF   3

/* NDEV_STATUS codes (status_error_codes.h) that this client maps.
   Success is reported as 0 over SIO and as 1 elsewhere -- see st_ok() in
   net.c. GM_NOREPLY is ours, not the device's: it means the status call
   itself failed, which is distinct from the device answering "0 bytes". */
#define GM_OK           1
#define GM_NOREPLY      0xFF
#define GM_EOF          136     /* buffer drained -- normal, not an error */
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

/* date.c -- render a wire timestamp into ENT_DATE_LEN bytes as "Aug 28 14:32",
   or "Aug 28  2024" for a message outside gm_year. Pure: no platform, no
   network, no device call. tests/hosttest.c exercises all of it. */
void          date_fmt(char *dst, const uint8_t ts[8]);

/* clock.c -- what date.c needs from the device, read once at boot. gm_tzoff is
   minutes east of UTC and gm_year the current year, or 0 for "unknown", which
   is also what they stay at if the FujiNet has no clock registered: dates then
   read as UTC and always carry a time. */
extern int          gm_tzoff;
extern unsigned int gm_year;
void          clock_load(void);

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

/* Portable key codes returned by plat_getkey(). */
#define K_NONE      0
#define K_UP        1
#define K_DOWN      2
#define K_LEFT      3
#define K_RIGHT     4
#define K_ENTER     5
#define K_BACK      6
#define K_REFRESH   7
#define K_QUIT      8

void          plat_init(void);
void          plat_shutdown(void);

/* Bracket every network and fuji device call. On the Atari this suppresses
   display list interrupts, which would otherwise steal cycles from a
   timing-critical SIO transfer. */
void          plat_net_begin(void);
void          plat_net_end(void);

unsigned char plat_getkey(void);        /* blocks, returns a K_* code */
void          plat_anykey(void);        /* blocks until any key */

/* Busy overlay reasons. */
#define BUSY_INDEX  1
#define BUSY_BODY   2

void          ui_splash(void);
void          ui_notfound(void);
void          ui_busy(unsigned char reason);
void          ui_error(unsigned char code);
void          ui_inbox(void);                   /* full repaint */
void          ui_inbox_sel(unsigned char from, unsigned char to);
void          ui_message(unsigned int top);     /* header + body window */

#endif /* GMAIL_H */
