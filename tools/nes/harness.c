/*
 * harness.c -- drive the Gmail client's NES image headless, in
 * fujinet-go-nes-desktop's core: MesenCE with the FujiNet cartridge board and
 * the in-process FujiNet. Adapted from netcat's (~/Workspace/netcat/nes).
 *
 *   harness [--kbd none|fb|subor] [--labels FILE] [--grant FNCONFIG] ROM < script
 *
 * --grant copies the [GoogleDrive] section -- the Google grant the GMAIL
 * adapter refreshes its token from -- out of an existing FujiNet's
 * fnconfig.ini into this run's own throwaway one, so a capture can read a
 * real mailbox. Nothing else of that FujiNet's configuration comes along.
 *
 * The script is one command per line:
 *   sleep MS            let the machine run
 *   idle MS             wait until the program has spent its GM_FAKE_KEYS
 *                       and the screen has caught up (fails on timeout)
 *   settle MS           wait until the screen has caught up
 *   until TEXT MS       wait until TEXT is on the screen (fails on timeout)
 *   expect TEXT         fail unless TEXT is on the screen now
 *   absent TEXT         fail if TEXT is on the screen
 *   type TEXT           type on the attached keyboard; \n RETURN, \e ESC,
 *                       \b DEL/backspace, \t TAB
 *   key NAME            return esc bs up down left right home stop
 *   pad NAME            tap a controller button: a b select start up down
 *                       left right
 *   dump                print the screen as text, with each two-row band's
 *                       palette in the margin
 *   check               compare the frame the PPU drew against the text the
 *                       program meant: every cell's ink pixels where its glyph
 *                       says, every paper pixel paper (fails on a mismatch)
 *   shot FILE           save a screenshot (PPM)
 *   mem ADDR N          CPU memory, in hex
 *   ppu ADDR N          PPU memory (as mapped now), in hex
 *   oam N               the PPU's own first N sprites (y tile attr x)
 *   reset               the console's RESET button
 *
 * "The screen" is the program's own text screen, scr_txt in WRAM, found by
 * its label in the linker's label file (build/gmail-nes.lbl by default) --
 * the bitmap has no character codes to read back. "check" is what proves the
 * bitmap matches it: it renders each cell from the font in the ROM and
 * compares the pixels.
 */

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nessession.h"
#include "nesdebug.h"

static nessession *S;
static nesdebug *D;
static int kbd_type;
static int failures;
static FILE *out;                   /* the real stdout: FujiNet captures fd 1 */

static unsigned a_scr_txt, a_dirty_lo, a_att, a_font_l1, a_inp_idle, a_stage;

static void sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static unsigned label(const char *path, const char *name)
{
    char line[256], want[128];
    FILE *f = fopen(path, "r");
    unsigned a = 0;
    if (!f) return 0;
    snprintf(want, sizeof want, ".%s", name);
    while (fgets(line, sizeof line, f)) {
        unsigned v;
        char n[128];
        if (sscanf(line, "al %x %127s", &v, n) == 2 && !strcmp(n, want)) { a = v; break; }
    }
    fclose(f);
    return a;
}

/* ---- the screen ------------------------------------------------------- */

#define ROWS 28
#define COLS 64

static uint8_t txt[ROWS * COLS], att[64];

static void snap(void)
{
    nesdebug_read(D, (uint16_t)a_scr_txt, txt, ROWS * COLS);
    nesdebug_read(D, (uint16_t)a_att, att, 64);
}

/* the palette of text row r's band at column c */
static int band_pal(int r, int c)
{
    int g = r / 2, a = (g + 1) / 2, sh = ((g & 1) ? 0 : 4) + (((c / 4) & 1) << 1);
    return (att[a * 8 + c / 8] >> sh) & 3;
}

static const char *glyph(uint8_t c)
{
    static char b[2];
    static const char *own[12] = { " ", "•", "◄", "►", "▲", "▼", "│", "─", "╶", "╴", "█", "…" };
    if (c < 12) return own[c];
    if (c < 32 || c > 126) return "?";
    b[0] = (char)c; b[1] = 0;
    return b;
}

static void screen_text(char *o, size_t sz)
{
    size_t n = 0;
    snap();
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            const char *s = glyph(txt[r * COLS + c]);
            size_t l = strlen(s);
            if (n + l + 2 < sz) { memcpy(o + n, s, l); n += l; }
        }
        if (n + 2 < sz) o[n++] = '\n';
    }
    o[n] = 0;
}

static void dump(void)
{
    static char buf[16384];
    static const char pn[] = "PSGR";        /* paper, selection, grey, red */
    char *line = buf;
    screen_text(buf, sizeof buf);
    for (int r = 0; r < ROWS; r++) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = 0;
        fprintf(out, "%2d %c|%s|\n", r, pn[band_pal(r, 32)], line);
        line = nl + 1;
    }
}

static int on_screen(const char *text)
{
    static char buf[16384];
    screen_text(buf, sizeof buf);
    return strstr(buf, text) != NULL;
}

/* Has the program's screen reached the PPU? dirty_lo all $FF, stage free. */
static int settled(void)
{
    uint8_t d[ROWS], st;
    nesdebug_read(D, (uint16_t)a_dirty_lo, d, ROWS);
    nesdebug_read(D, (uint16_t)a_stage, &st, 1);
    for (int i = 0; i < ROWS; i++) if (d[i] != 0xFF) return 0;
    return st == 0;
}

static int idle(void)
{
    uint8_t v = 0;
    if (a_inp_idle) nesdebug_read(D, (uint16_t)a_inp_idle, &v, 1);
    return v && settled();
}

static void shot(const char *path)
{
    static uint32_t px[256 * 240];
    uint64_t serial = 0;
    int h = 240;
    FILE *f;
    nessession_copy_frame(S, px, &h, &serial);
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n256 %d\n255\n", h);
    for (int i = 0; i < 256 * h; i++) {
        uint8_t rgb[3] = { (uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i] };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/*
 * The frame against the text: each cell's glyph, rows 1-6, from the font
 * tables in R7 (always mapped), against the pixels at (4c, 8 + 8r). A cell
 * passes when its ink pixels all share one colour, its paper pixels all
 * share another, and the two differ (or the glyph is blank and the paper is
 * uniform). Cells under a sprite on the screen are skipped, found from the
 * program's OAM page at $0200 -- and so are rows skip_lo..skip_hi, if given.
 */
static void check(const char *arg)
{
    static uint8_t covered[ROWS][COLS];
    static uint32_t px[256 * 240];
    uint8_t font[6][128];
    uint64_t serial = 0;
    int h = 240, bad = 0, skip_lo = -1, skip_hi = -1;
    sscanf(arg, "%d %d", &skip_lo, &skip_hi);
    snap();
    for (int y = 0; y < 6; y++)
        nesdebug_read(D, (uint16_t)(a_font_l1 + y * 128), font[y], 128);
    {
        uint8_t oam[256];
        memset(covered, 0, sizeof covered);
        nesdebug_read(D, 0x0200, oam, 256);
        for (int i = 0; i < 256; i += 4) {
            int y = oam[i] + 1, x = oam[i + 3];
            if (oam[i] >= 0xEF) continue;
            for (int r = 0; r < ROWS; r++) {
                int top = 8 + 8 * r;
                if (top + 8 <= y || top >= y + 16) continue;
                for (int c = x / 4; c <= (x + 7) / 4 && c < COLS; c++)
                    covered[r][c] = 1;
            }
        }
    }
    nessession_copy_frame(S, px, &h, &serial);
    for (int r = 0; r < ROWS; r++) {
        if (r >= skip_lo && r <= skip_hi) continue;
        for (int c = 0; c < COLS; c++) {
            uint8_t ch = txt[r * COLS + c] & 0x7F;
            uint32_t ink = 0, paper = 0;
            int have_ink = 0, have_paper = 0, ok = 1;
            if (covered[r][c]) continue;
            for (int y = 1; y < 7 && ok; y++) {
                uint8_t g = font[y - 1][ch] >> 4;
                for (int x = 0; x < 4; x++) {
                    uint32_t p = px[(8 + 8 * r + y) * 256 + 4 * c + x] & 0xFFFFFF;
                    if (g & (8 >> x)) {
                        if (!have_ink) { ink = p; have_ink = 1; }
                        else if (p != ink) ok = 0;
                    } else {
                        if (!have_paper) { paper = p; have_paper = 1; }
                        else if (p != paper) ok = 0;
                    }
                }
            }
            if (have_ink && have_paper && ink == paper) ok = 0;
            if (!ok) {
                if (bad < 8)
                    fprintf(out, "check: row %d col %d ('%s') does not match\n",
                            r, c, glyph(ch));
                bad++;
            }
        }
    }
    if (bad) { fprintf(out, "FAIL: check: %d cell(s) differ\n", bad); failures++; }
    else fprintf(out, "ok: check\n");
}

/* ---- typing ----------------------------------------------------------- */

/* MesenCE's Family BASIC key indices (core/src/keyboard.c's enum) */
enum {
    FB_A = 0, FB_0 = 26, FB_RETURN = 36, FB_SPACE, FB_DEL, FB_INS, FB_ESC,
    FB_CTRL, FB_RSHIFT, FB_LSHIFT, FB_RBRACKET, FB_LBRACKET,
    FB_UP, FB_DOWN, FB_LEFT, FB_RIGHT, FB_DOT, FB_COMMA, FB_COLON,
    FB_SEMICOLON, FB_UNDERSCORE, FB_SLASH, FB_MINUS, FB_CARET,
    FB_F1, FB_YEN = FB_F1 + 8, FB_STOP, FB_AT, FB_GRPH, FB_CLRHOME, FB_KANA,
};

/* a character as (key, shifted) on the Family BASIC keyboard, by its caps */
static int fb_key(int ch, int *shift)
{
    static const struct { char c; int k, s; } map[] = {
        { ' ', FB_SPACE, 0 }, { '.', FB_DOT, 0 }, { ',', FB_COMMA, 0 },
        { ':', FB_COLON, 0 }, { ';', FB_SEMICOLON, 0 }, { '/', FB_SLASH, 0 },
        { '-', FB_MINUS, 0 }, { '^', FB_CARET, 0 }, { '@', FB_AT, 0 },
        { '[', FB_LBRACKET, 0 }, { ']', FB_RBRACKET, 0 }, { '\\', FB_YEN, 0 },
        { '_', FB_UNDERSCORE, 0 }, { '>', FB_DOT, 1 }, { '<', FB_COMMA, 1 },
        { '*', FB_COLON, 1 }, { '+', FB_SEMICOLON, 1 }, { '?', FB_SLASH, 1 },
        { '=', FB_MINUS, 1 }, { '~', FB_CARET, 1 }, { '`', FB_AT, 1 },
        { '{', FB_LBRACKET, 1 }, { '}', FB_RBRACKET, 1 }, { '|', FB_YEN, 1 },
        { '!', FB_0 + 1, 1 }, { '"', FB_0 + 2, 1 }, { '#', FB_0 + 3, 1 },
        { '$', FB_0 + 4, 1 }, { '%', FB_0 + 5, 1 }, { '&', FB_0 + 6, 1 },
        { '\'', FB_0 + 7, 1 }, { '(', FB_0 + 8, 1 }, { ')', FB_0 + 9, 1 },
        { '\n', FB_RETURN, 0 }, { 27, FB_ESC, 0 }, { 8, FB_DEL, 0 },
    };
    *shift = 0;
    if (ch >= 'a' && ch <= 'z') return FB_A + ch - 'a';
    if (ch >= 'A' && ch <= 'Z') { *shift = 1; return FB_A + ch - 'A'; }
    if (ch >= '0' && ch <= '9') return FB_0 + ch - '0';
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (map[i].c == ch) { *shift = map[i].s; return map[i].k; }
    return -1;
}

static void tap(int key, int mod)
{
    if (mod >= 0) { nessession_keyboard_press(S, mod, 1); sleep_ms(40); }
    nessession_keyboard_press(S, key, 1);
    sleep_ms(60);
    nessession_keyboard_press(S, key, 0);
    if (mod >= 0) { sleep_ms(30); nessession_keyboard_press(S, mod, 0); }
    sleep_ms(60);
}

static void type_text(const char *t)
{
    for (; *t; t++) {
        int ch = (unsigned char)*t, shift, k;
        if (ch == '\\' && t[1]) {
            t++;
            ch = *t == 'n' ? '\n' : *t == 'e' ? 27 : *t == 'b' ? 8 : *t == 't' ? 9 : *t;
        }
        if (kbd_type == NES_KBD_SUBOR) {
            /* the desktop's own positional map serves for the Subor */
            uint32_t ks = ch == '\n' ? NES_KEYSYM_RETURN : ch == 27 ? NES_KEYSYM_ESCAPE :
                          ch == 8 ? NES_KEYSYM_BACKSPACE : ch == 9 ? NES_KEYSYM_TAB : (uint32_t)ch;
            int upper = ch >= 'A' && ch <= 'Z';
            k = nessession_keyboard_index_for_keysym(kbd_type, ks);
            if (k < 0) { fprintf(stderr, "harness: no Subor key for '%c'\n", ch); continue; }
            tap(k, upper ? nessession_keyboard_index_for_keysym(kbd_type, NES_KEYSYM_LSHIFT) : -1);
            continue;
        }
        k = fb_key(ch, &shift);
        if (k < 0) { fprintf(stderr, "harness: no key for '%c'\n", ch); continue; }
        tap(k, shift ? FB_LSHIFT : -1);
    }
}

static void key_named(const char *n)
{
    int k = -1;
    if (kbd_type == NES_KBD_SUBOR) {
        uint32_t ks = 0;
        if (!strcmp(n, "return")) ks = NES_KEYSYM_RETURN;
        else if (!strcmp(n, "esc")) ks = NES_KEYSYM_ESCAPE;
        else if (!strcmp(n, "bs")) ks = NES_KEYSYM_BACKSPACE;
        else if (!strcmp(n, "up")) ks = NES_KEYSYM_UP;
        else if (!strcmp(n, "down")) ks = NES_KEYSYM_DOWN;
        else if (!strcmp(n, "left")) ks = NES_KEYSYM_LEFT;
        else if (!strcmp(n, "right")) ks = NES_KEYSYM_RIGHT;
        else if (n[0] == 'f' && n[1] >= '1' && n[1] <= '8') ks = NES_KEYSYM_F1 + (n[1] - '1');
        else if (!strcmp(n, "home")) ks = 0xff50;
        else if (!strcmp(n, "stop")) ks = 0xff13;     /* Pause: the Subor's STOP */
        k = ks ? nessession_keyboard_index_for_keysym(kbd_type, ks) : -1;
    } else {
        if (!strcmp(n, "return")) k = FB_RETURN;
        else if (!strcmp(n, "esc")) k = FB_ESC;
        else if (!strcmp(n, "bs")) k = FB_DEL;
        else if (!strcmp(n, "up")) k = FB_UP;
        else if (!strcmp(n, "down")) k = FB_DOWN;
        else if (!strcmp(n, "left")) k = FB_LEFT;
        else if (!strcmp(n, "right")) k = FB_RIGHT;
        else if (!strcmp(n, "home")) k = FB_CLRHOME;
        else if (!strcmp(n, "stop")) k = FB_STOP;
        else if (n[0] == 'f' && n[1] >= '1' && n[1] <= '8') k = FB_F1 + (n[1] - '1');
    }
    if (k < 0) { fprintf(stderr, "harness: no key '%s'\n", n); return; }
    tap(k, -1);
}

static void pad(const char *n)
{
    static const char *names[] = { "up", "down", "left", "right", "a", "b", "select", "start" };
    int withb = 0;
    if (!strncmp(n, "b+", 2)) { withb = 1; n += 2; }
    for (int i = 0; i < 8; i++) {
        if (strcmp(n, names[i])) continue;
        if (withb) { nessession_press(S, NES_TARGET_PORT(0, NES_ACT_B), 1); sleep_ms(50); }
        nessession_press(S, NES_TARGET_PORT(0, i), 1);
        sleep_ms(80);
        nessession_press(S, NES_TARGET_PORT(0, i), 0);
        if (withb) { sleep_ms(30); nessession_press(S, NES_TARGET_PORT(0, NES_ACT_B), 0); }
        sleep_ms(80);
        return;
    }
    fprintf(stderr, "harness: no button '%s'\n", n);
}

int main(int argc, char **argv)
{
    char cfg[512], data[512], line[1024];
    const char *rom = NULL;
    nessession_paths p;
    nessession_start_opts o;

    const char *labels = "build/gmail-nes.lbl";
    const char *grant = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--labels") && i + 1 < argc) labels = argv[++i];
        else if (!strcmp(argv[i], "--grant") && i + 1 < argc) grant = argv[++i];
        else if (!strcmp(argv[i], "--kbd") && i + 1 < argc) {
            i++;
            kbd_type = !strcmp(argv[i], "fb") ? NES_KBD_FAMILY_BASIC :
                       !strcmp(argv[i], "subor") ? NES_KBD_SUBOR : NES_KBD_NONE;
        } else rom = argv[i];
    }
    out = fdopen(dup(1), "w");
    a_scr_txt = label(labels, "_scr_txt");
    a_dirty_lo = label(labels, "_dirty_lo");
    a_att = label(labels, "_att");
    a_font_l1 = label(labels, "_font_l1");
    a_inp_idle = label(labels, "_inp_idle");
    a_stage = 0x0300;                        /* st_state: STAGE's first byte */
    if (!a_scr_txt || !a_dirty_lo || !a_att || !a_font_l1) {
        fprintf(stderr, "harness: no labels in %s\n", labels);
        return 2;
    }
    static char romabs[4096];
    if (rom && realpath(rom, romabs)) rom = romabs;
    if (!rom) { fprintf(stderr, "usage: harness [--kbd none|fb|subor] ROM < script\n"); return 2; }

    snprintf(cfg, sizeof cfg, "/tmp/nes-gmail-cfg-%ld", (long)getpid());
    snprintf(data, sizeof data, "/tmp/nes-gmail-data-%ld", (long)getpid());
    mkdir(cfg, 0755);
    mkdir(data, 0755);
    if (grant) {
        /* the desktop's own configuration -- the BoIP link and the rest --
           with the grant's [GoogleDrive] section in place of its own */
        char path[600], base[600], l[1024];
        const char *home = getenv("HOME");
        FILE *o, *in;
        int keep;
        snprintf(base, sizeof base, "%s/.local/share/fujinet-go-nes/fujinet/fnconfig.ini",
                 home ? home : "");
        snprintf(path, sizeof path, "%s/fujinet", data);
        mkdir(path, 0755);
        snprintf(path, sizeof path, "%s/fujinet/fnconfig.ini", data);
        o = fopen(path, "w");
        in = fopen(base, "r");
        if (!o || !in) { fprintf(stderr, "harness: --grant: no %s\n", base); return 2; }
        keep = 1;
        while (fgets(l, sizeof l, in)) {
            if (l[0] == '[') keep = strncmp(l, "[GoogleDrive]", 13) != 0;
            if (keep) fputs(l, o);
        }
        fclose(in);
        in = fopen(grant, "r");
        if (!in) { fprintf(stderr, "harness: --grant %s: cannot read\n", grant); return 2; }
        keep = 0;
        while (fgets(l, sizeof l, in)) {
            if (l[0] == '[') keep = !strncmp(l, "[GoogleDrive]", 13);
            if (keep) fputs(l, o);
        }
        fclose(in);
        fclose(o);
    }
    memset(&p, 0, sizeof p);
    p.config_dir = cfg;
    p.data_dir = data;
    S = nessession_new(&p);
    if (!S) return 1;
    nessession_default_opts(S, &o);
    o.enable_audio = 0;
    o.enable_gamepad = 0;
    o.enable_fujinet = 1;
    o.keyboard = kbd_type;
    if (nessession_start(S, &o) != 0) {
        fprintf(stderr, "harness: start: %s\n", nessession_last_error(S));
        return 1;
    }
    if (!nessession_fujinet_running(S))
        fprintf(stderr, "harness: warning: no FujiNet runtime\n");
    D = nessession_debugger(S);
    if (nessession_load_cart(S, rom) != 0) {
        fprintf(out, "harness: load %s: %s\n", rom, nessession_last_error(S));
        fflush(out);
        return 1;
    }
    nesdebug_attach(D);             /* once, then run: attaching stops the machine */
    nesdebug_resume(D);
    nessession_set_keyboard(S, kbd_type);
    nessession_set_keyboard_mode(S, kbd_type != NES_KBD_NONE);

    while (fgets(line, sizeof line, stdin)) {
        char *cmd = line, *arg;
        line[strcspn(line, "\r\n")] = 0;
        while (*cmd == ' ') cmd++;
        if (!*cmd || *cmd == '#') continue;
        arg = strchr(cmd, ' ');
        if (arg) *arg++ = 0; else arg = cmd + strlen(cmd);

        if (!strcmp(cmd, "sleep")) sleep_ms(atoi(arg));
        else if (!strcmp(cmd, "idle") || !strcmp(cmd, "settle")) {
            int t = atoi(arg), waited = 0, want_idle = cmd[0] == 'i';
            if (t <= 0) t = 10000;
            while (!(want_idle ? idle() : settled()) && waited < t) { sleep_ms(50); waited += 50; }
            if (waited >= t) {
                fprintf(out, "FAIL: not %s after %d ms\n", want_idle ? "idle" : "settled", t);
                failures++;
            } else fprintf(out, "ok: %s\n", want_idle ? "idle" : "settled");
        } else if (!strcmp(cmd, "check")) check(arg);
        else if (!strcmp(cmd, "until")) {
            char *ms = strrchr(arg, ' ');
            int t = ms ? atoi(ms + 1) : 5000, waited = 0;
            if (ms) *ms = 0;
            while (!on_screen(arg) && waited < t) { sleep_ms(100); waited += 100; }
            if (!on_screen(arg)) {
                fprintf(out, "FAIL: \"%s\" not on the screen after %d ms\n", arg, t);
                dump();
                failures++;
            } else fprintf(out, "ok: \"%s\"\n", arg);
        } else if (!strcmp(cmd, "expect")) {
            if (on_screen(arg)) fprintf(out, "ok: \"%s\"\n", arg);
            else { fprintf(out, "FAIL: \"%s\" not on the screen\n", arg); dump(); failures++; }
        } else if (!strcmp(cmd, "absent")) {
            if (!on_screen(arg)) fprintf(out, "ok: no \"%s\"\n", arg);
            else { fprintf(out, "FAIL: \"%s\" is on the screen\n", arg); dump(); failures++; }
        } else if (!strcmp(cmd, "type")) type_text(arg);
        else if (!strcmp(cmd, "key")) key_named(arg);
        else if (!strcmp(cmd, "ctrl")) {
            int k = kbd_type == NES_KBD_SUBOR ?
                nessession_keyboard_index_for_keysym(kbd_type, (uint32_t)(arg[0] | 0x20)) :
                FB_A + ((arg[0] | 0x20) - 'a');
            int c = kbd_type == NES_KBD_SUBOR ?
                nessession_keyboard_index_for_keysym(kbd_type, NES_KEYSYM_LCTRL) : FB_CTRL;
            tap(k, c);
        } else if (!strcmp(cmd, "pad")) pad(arg);
        else if (!strcmp(cmd, "dump")) dump();
        else if (!strcmp(cmd, "shot")) shot(arg);
        else if (!strcmp(cmd, "reset")) nessession_reset_game(S);
        else if (!strcmp(cmd, "oam")) {
            nesdebug_sprite sp[64];
            int n = atoi(arg);
            nesdebug_oam_get(D, sp);
            for (int i = 0; i < n && i < 64; i++)
                fprintf(out, "%s%02X %02X %02X %02X", i % 4 ? "  " : (i ? "\n" : ""),
                        sp[i].y, sp[i].tile, sp[i].attr, sp[i].x);
            fprintf(out, "\n");
        }
        else if (!strcmp(cmd, "ppu")) {
            unsigned a = 0, n = 16;
            uint8_t b[1024];
            sscanf(arg, "%x %u", &a, &n);
            if (n > 1024) n = 1024;
            nesdebug_ppu_read(D, (uint16_t)a, b, (int)n);
            for (unsigned i = 0; i < n; i++)
                fprintf(out, "%s%02X", i % 32 ? " " : (i ? "\n" : ""), b[i]);
            fprintf(out, "\n");
        }
        else if (!strcmp(cmd, "mem")) {
            unsigned a = 0, n = 16;
            uint8_t b[256];
            sscanf(arg, "%x %u", &a, &n);
            if (n > 256) n = 256;
            nesdebug_read(D, (uint16_t)a, b, (int)n);
            for (unsigned i = 0; i < n; i++)
                fprintf(out, "%s%02X", i % 16 ? " " : (i ? "\n" : ""), b[i]);
            fprintf(out, "\n");
        }
        else if (!strcmp(cmd, "fps")) {
            /* how fast the program's frame counter runs, over a second */
            uint8_t a, b;
            unsigned addr = 0;
            sscanf(arg, "%x", &addr);
            nesdebug_read(D, (uint16_t)addr, &a, 1);
            sleep_ms(1000);
            nesdebug_read(D, (uint16_t)addr, &b, 1);
            fprintf(out, "fps: %d\n", (uint8_t)(b - a));
        }
        else if (!strcmp(cmd, "echo")) fprintf(out, "%s\n", arg);
        else fprintf(stderr, "harness: unknown command '%s'\n", cmd);
        fflush(out);
    }

    fprintf(out, "%d failure(s)\n", failures);
    fflush(out);
    /* FujiNet's threads do not survive an orderly exit; skip the teardown */
    _exit(failures ? 1 : 0);
}
