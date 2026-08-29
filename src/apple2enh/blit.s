;
; The inner loop of the 80-column blitter, for screen.c.
;
; Not screen.s, deliberately: the build globs *.c and *.s out of one directory
; and maps both onto <name>.o, so a .c and a .s sharing a basename collide in
; the link. src/atari/ splits dli.c from dlihw.s for the same reason.
;
; Even columns live in auxiliary memory and odd columns in main, so a run of
; text is two interleaved runs of bytes in two banks. Doing that a character at
; a time from C would mean a bank switch per character; this does the odd half
; and then the even half, so one run costs one switch.
;
; The technique is cc65's own (libsrc/apple2/cputc.s): with 80STORE on, a touch
; of HISCR pages $0400-$07FF to aux and LOWSCR puts it back. Interrupts are
; held off across the aux window because MSLOT and the other low-memory
; vectors an interrupt handler expects are not there while it is open.
;
; The caller stages the run in _scr_buf and sets the three parameters, rather
; than passing them: absolute,X is the only indexed mode that can read the
; source while (zp),y writes the destination, and absolute,X needs the source
; at a link-time address.
;
;       void scr_blit (void);
;

        .export         _scr_blit
        .import         _scr_buf, _scr_row, _scr_col, _scr_len

        .include        "zeropage.inc"
        .include        "apple2.inc"

        .code

_scr_blit:
        ldx     _scr_row
        lda     rowlo,x
        sta     ptr1
        lda     rowhi,x
        sta     ptr1+1

        ;
        ; Odd columns, in main memory.
        ;
        ; Column c sits at byte offset c/2 in its own bank, so the first odd
        ; column at or after col is always at offset col/2 -- whether that is
        ; col itself (col odd, source index 0) or col+1 (col even, index 1).
        ;
        lda     _scr_col
        lsr                             ; A = col/2, carry = col & 1
        tay                             ; Y = destination offset
        ldx     #0                      ; X = source index
        bcs     :+
        inx                             ; col even: start one character in
:       jsr     run

        ;
        ; Even columns, in aux. The mirror of the above: the first even column
        ; at or after col is col itself when col is even, and col+1 -- which
        ; has rolled into the next byte -- when it is odd.
        ;
        lda     _scr_col
        lsr
        tay
        ldx     #0
        bcc     :+
        iny                             ; col odd: the next byte along
        inx                             ; and one character in
:
        php
        sei
        bit     HISCR                   ; $0400-$07FF -> aux
        jsr     run
        bit     LOWSCR                  ; and back
        plp
        rts

; Y = destination byte offset, X = index into _scr_buf. Both walk forward, the
; source at two characters per destination byte, until X runs off _scr_len.
run:    cpx     _scr_len
        bcs     done
        lda     _scr_buf,x
        sta     (ptr1),y
        iny
        inx
        inx
        bne     run                     ; len is at most 80, so X cannot wrap
done:   rts

        .rodata

; Text page 1, row by row: $0400 + (row & 7) * $80 + (row >> 3) * $28. Spelled
; out rather than computed -- it is 48 bytes against a shift-and-add on every
; single field write.
rowlo:  .byte   $00, $80, $00, $80, $00, $80, $00, $80
        .byte   $28, $A8, $28, $A8, $28, $A8, $28, $A8
        .byte   $50, $D0, $50, $D0, $50, $D0, $50, $D0
rowhi:  .byte   $04, $04, $05, $05, $06, $06, $07, $07
        .byte   $04, $04, $05, $05, $06, $06, $07, $07
        .byte   $04, $04, $05, $05, $06, $06, $07, $07
