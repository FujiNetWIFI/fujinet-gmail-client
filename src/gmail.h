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

/* Entries fetched per range. Each one costs the adapter an extra upstream
   HTTPS round trip inside the open(), so this is deliberately modest. */
#define IDX_MAX         16

/* Wrapped body rows kept before we give up and set the truncation flag.
   300 rows is ~12000 characters of message text and still leaves ~8K of the
   Atari's BSS budget free; check r2r/atari/gmail.map before raising it. */
#define BODY_ROWS       300
#define BODY_COLS       40
#define BODY_STRIDE     (BODY_COLS + 1)

/* Longest raw line accumulated before a hard flush through the wrapper. */
#define LINE_CAP        200

#define ENT_NUM_LEN     12      /* msgNum as ASCII decimal, NUL terminated */
#define ENT_NAME_LEN    32
#define ENT_SUBJ_LEN    64

/* ------------------------------------------------------------------ */
/* Wire format                                                         */
/* ------------------------------------------------------------------ */

/*
 * Mailbox.h's MailIndexItem, #pragma pack(1), 220 bytes. The firmware carries
 * a static_assert(sizeof(MailIndexItem) == 220) as a tripwire against this
 * layout changing -- if a firmware update trips it, this struct is what moves.
 *
 * The wire is little-endian and so is the 6502, and cc65 inserts no struct
 * padding, so a record can be read straight into this.
 */
struct wire_rec {
    uint32_t msgnum;            /*   0    4  */
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

/* fn_default_timeout is in 64-frame ticks. 15 (the library default) is ~16s;
   a GMAIL index open is ~19 sequential upstream HTTPS round trips. */
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

/* hwm.c -- read/unread high-water mark, persisted in a FujiNet appkey. */
void          hwm_load(void);
void          hwm_flags(void);                      /* recompute all unread bits */
void          hwm_update(const uint8_t *ts);        /* advance + persist if newer */

/* ------------------------------------------------------------------ */
/* Platform backend -- implemented per target under src/<platform>/     */
/* ------------------------------------------------------------------ */

/* Visible body rows in the message reader, and list rows in the inbox.
   A platform with a different screen geometry overrides these. */
#ifndef MSG_ROWS
#define MSG_ROWS    18
#endif
#ifndef LIST_ROWS
#define LIST_ROWS   IDX_MAX
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
