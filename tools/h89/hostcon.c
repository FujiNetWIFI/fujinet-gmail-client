/*
 * The H89 console, on a host: for tools/h89/hostrun.sh, which builds the
 * whole program natively with canned data (GM_FAKE_DATA) to look at exactly
 * what it sends the H19.
 *
 * con_out() writes each byte to stdout untouched. The keys come from the
 * script in $H89_KEYS, one token at a time once the program has gone quiet
 * long enough: a key name (f1..f5 blue red gray erase up down left right
 * home esc return delete backspace tab), "type:TEXT", "wait:N" (N more
 * ticks of quiet), "shot", which puts a marker in the output for
 * h19model.py to print the screen at, or "bell", a shot taken shortly after
 * the program next rings the bell. When the script runs out the program is
 * walked back out to the inbox and sent a Q.
 *
 * The clock is a counter that ticks once per read, so a program waiting on
 * it moves time along.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t host_ticks;

uint16_t host_tick(void)
{
    return host_ticks++;
}

void con_init(void) {}
void con_exit(void) {}

/* Reads since the last byte out: the program is idle once it has asked for
   a key this many times without drawing anything in between. */
#define IDLE_READS 300
static unsigned idle;

/* "bell": waiting for one (1), then counting reads after it (2..) */
static unsigned bell;
static unsigned long bell_wait;
#define BELL_READS 40

void con_out(uint8_t c)
{
    putchar(c);
    idle = 0;
    if (c == 0x07 && bell == 1)
        bell = 2;
}

static const char *script;
static char pend[64];
static int  plen, ppos;
static uint16_t quiet_until;
static int  done;

static const struct { const char *name; const char *seq; } keys[] = {
    { "f1", "\033S" }, { "f2", "\033T" }, { "f3", "\033U" }, { "f4", "\033V" },
    { "f5", "\033W" }, { "blue", "\033P" }, { "red", "\033Q" },
    { "gray", "\033R" }, { "erase", "\033J" }, { "up", "\033A" },
    { "down", "\033B" }, { "right", "\033C" }, { "left", "\033D" },
    { "home", "\033H" }, { "esc", "\033" }, { "return", "\r" },
    { "delete", "\177" }, { "backspace", "\b" }, { "tab", "\t" },
    { "pgup", "\033N" }, { "pgdn", "\033M" },
};

/* The next token's bytes into pend, or a quiet wait; 0 at the end. */
static int next_token(void)
{
    char tok[64];
    int  n = 0;
    unsigned i;

    if (!script)
        script = getenv("H89_KEYS") ? getenv("H89_KEYS") : "";
    while (*script == ' ')
        script++;
    if (!*script)
        return 0;
    while (*script && *script != ' ' && n < 63)
        tok[n++] = *script++;
    tok[n] = 0;

    plen = ppos = 0;
    if (strcmp(tok, "bell") == 0) {
        bell = 1;
        return 1;
    }
    if (strcmp(tok, "shot") == 0) {
        fflush(stdout);
        printf("\377SHOT\377");
        return 1;
    }
    if (strncmp(tok, "wait:", 5) == 0) {
        quiet_until = (uint16_t) (host_ticks + atoi(tok + 5));
        return 1;
    }
    if (strncmp(tok, "type:", 5) == 0) {
        for (i = 5; tok[i]; i++)
            pend[plen++] = tok[i] == '_' ? ' ' : tok[i];
        return 1;
    }
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (strcmp(tok, keys[i].name) == 0) {
            strcpy(pend, keys[i].seq);
            plen = (int) strlen(pend);
            return 1;
        }
    fprintf(stderr, "hostcon: unknown key %s\n", tok);
    exit(2);
}

int con_in(void)
{
    host_ticks++;
    if (ppos < plen)
        return (unsigned char) pend[ppos++];
    if (bell) {
        if (bell == 1 && ++bell_wait > 20000000u) {
            fprintf(stderr, "hostcon: no bell\n");
            bell = 0;
        }
        if (bell > 1 && ++bell > BELL_READS) {
            bell = 0;
            fflush(stdout);
            printf("\377SHOT\377");
        }
        return -1;
    }
    /* Let the program settle between keys: a token is taken only after a
       stretch of reads with nothing drawn, which is a program idling. */
    if (++idle < IDLE_READS || (int16_t) (host_ticks - quiet_until) < 0)
        return -1;
    idle = 0;
    if (!next_token()) {
        /* Walk back out to the inbox, where Q quits: ESC leaves the reader,
           and leaves a form -- through its send? question, which N answers
           -- and the second ESC leaves a reader the form was opened from. */
        static const char out[] = "\033N\033Q";

        if (done > 50000) {
            fprintf(stderr, "hostcon: the program did not quit\n");
            exit(3);
        }
        pend[0] = out[done++ % 4];
        plen = 1;
        ppos = 0;
    }
    return -1;
}

/* z88dk's, which the host's C library does not have. */
char *ultoa(unsigned long v, char *buf, int radix)
{
    char tmp[12];
    int  n = 0, i = 0;

    do {
        tmp[n++] = (char) ('0' + v % radix);
        v /= radix;
    } while (v);
    while (n)
        buf[i++] = tmp[--n];
    buf[i] = 0;
    return buf;
}

char *utoa(unsigned int v, char *buf, int radix)
{
    return ultoa(v, buf, radix);
}

/* The fujinet-lib symbols a GM_FAKE_DATA build still links -- net.c's
   probe() and close paths, which canned data never reaches. */
uint8_t fn_device_error;

uint8_t network_close(const char *devicespec)
{
    (void) devicespec;
    return 0;
}

uint8_t network_status(const char *devicespec, uint16_t *bw, uint8_t *c,
                       uint8_t *err)
{
    (void) devicespec;
    *bw = 0;
    *c = 0;
    *err = 1;
    return 0;
}
