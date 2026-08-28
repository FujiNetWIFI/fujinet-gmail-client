;
; Blocking keyboard read.
;
; This is cc65's cgetc() minus its setcursor() call. setcursor() is unusable
; here: with the cursor switched off it writes OLDCHR back over the previous
; cursor cell and clears the inverse-video bit on the current one, both of
; which corrupt a screen we drew ourselves by blitting straight into screen
; memory. Calling KEYBDV directly avoids the screen entirely.
;
; unsigned char plat_key (void);   returns ATASCII in A
;

        .include        "atari.inc"

        .export         _plat_key

_plat_key:
        lda     #12                     ; read/write; KEYBDV is picky when
        sta     ICAX1Z                  ; called without going through CIO
        jsr     callkbd
        ldx     #0                      ; high byte of the return value
        rts

callkbd:
        lda     KEYBDV+5                ; get-character vector, pushed for RTS
        pha
        lda     KEYBDV+4
        pha
        rts
