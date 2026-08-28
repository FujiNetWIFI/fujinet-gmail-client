;
; Display list interrupt chain.
;
; GRAPHICS 0 gives one background and one text luminance for the whole frame,
; which is not enough for a Gmail-looking screen. Two DLIs split it into three
; bands: a light header (rows 0-2), a white message area (rows 3-22) and a red
; footer (row 23).
;
; Each interrupt arms the other one, so the chain cannot drift out of phase --
; whichever ran last always leaves the first one armed for the next frame. The
; header band needs no interrupt of its own: the OS vertical blank copies the
; COLOR1/COLOR2 shadows into the hardware before every frame, and dli.c keeps
; the header colors in those shadows.
;

        .export         _dli_hw_on, _dli_hw_off
        .export         _dli_vbi_install, _dli_vbi_remove
        .export         _dli_list_bg, _dli_list_fg
        .export         _dli_foot_bg, _dli_foot_fg

VDSLST  = $0200
VVBLKI  = $0222
ATRACT  = $004D
WSYNC   = $D40A
COLPF1  = $D017
COLPF2  = $D018
NMIEN   = $D40E
SETVBV  = $E45C
SYSVBV  = $E45F

        .bss

vbi_save:       .res    2

        .data

_dli_list_bg:   .byte   $0E
_dli_list_fg:   .byte   $00
_dli_foot_bg:   .byte   $38
_dli_foot_fg:   .byte   $0E

        .code

; Fires on the last scanline of row 2; its writes land on row 3.
dli_list:
        pha
        lda     _dli_list_fg
        sta     WSYNC
        sta     COLPF1
        lda     _dli_list_bg
        sta     COLPF2
        lda     #<dli_foot
        sta     VDSLST
        lda     #>dli_foot
        sta     VDSLST+1
        pla
        rti

; Fires on the last scanline of row 22; its writes land on row 23.
dli_foot:
        pha
        lda     _dli_foot_fg
        sta     WSYNC
        sta     COLPF1
        lda     _dli_foot_bg
        sta     COLPF2
        lda     #<dli_list
        sta     VDSLST
        lda     #>dli_list
        sta     VDSLST+1
        pla
        rti

; ----------------------------------------------------------------------
; void dli_hw_on (void);
;
; Arm the chain from the top and enable DLI + VBI NMIs.
; ----------------------------------------------------------------------

_dli_hw_on:
        lda     #$40                    ; VBI only while the vector moves
        sta     NMIEN
        lda     #<dli_list
        sta     VDSLST
        lda     #>dli_list
        sta     VDSLST+1
        lda     #$C0                    ; DLI + VBI
        sta     NMIEN
        rts

; ----------------------------------------------------------------------
; void dli_hw_off (void);
; ----------------------------------------------------------------------

_dli_hw_off:
        lda     #$40
        sta     NMIEN
        rts

; ----------------------------------------------------------------------
; Immediate vertical blank hook.
;
; Its only job is to keep ATRACT at zero. After about nine minutes without a
; keypress the OS starts rotating the color shadows to protect the CRT, which
; would dim the header band while the DLI-written bands below it stayed put --
; a screen that looks broken rather than idle. We cannot poll for this from C
; because the program spends its time blocked inside the keyboard handler.
; ----------------------------------------------------------------------

vbi:
        lda     #$00
        sta     ATRACT

        ; Re-arm the chain from the top of every frame. The two interrupts
        ; point at each other, which only stays in phase while exactly two of
        ; them fire per frame -- enabling NMIs mid-screen, as a repaint does,
        ; would otherwise leave the two swapped for good and paint the message
        ; area in the footer's colour. Doing it here makes that self-healing.
        lda     #<dli_list
        sta     VDSLST
        lda     #>dli_list
        sta     VDSLST+1

        jmp     SYSVBV                  ; on into the OS vertical blank

; void dli_vbi_install (void);
_dli_vbi_install:
        lda     VVBLKI
        sta     vbi_save
        lda     VVBLKI+1
        sta     vbi_save+1
        ldy     #<vbi
        ldx     #>vbi
        lda     #6                      ; 6 = immediate VBI vector
        jmp     SETVBV

; void dli_vbi_remove (void);
_dli_vbi_remove:
        ldy     vbi_save
        ldx     vbi_save+1
        lda     #6
        jmp     SETVBV
