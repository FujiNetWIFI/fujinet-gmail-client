;
; crt0.s -- the Gmail client on the NES: the iNES header, start-up, the
; frame's interrupts and the cross-bank call (src/nes/neshw.h).
;
; The screen is a bitmap of 28 text rows, every cell its own tile, and a
; pattern table covers only eight of those rows. MMC3's scanline counter
; interrupts where the table has to change (with the vertical scroll at 8,
; text row r is on lines 8 + 8r):
;
;   line   7  band 0 (rows 0-7):   R0/R1 = CHR banks 0/2
;         71  band 1 (rows 8-15):  4/6
;        135  band 2 (rows 16-23): 8/10
;        199  band 3 (rows 24-27): 12/14 -- 14 is the fixed tiles
;        231  everything off: from here to the end of the vertical blank the
;             PPU is free, and ppu_drain (ppu.s) copies across whatever the
;             program has changed, then the sprites go and show sets up the
;             next frame -- or the NMI does both, if it comes first
;
; Each switch lands a few dozen cycles into the first line of its band. That
; is harmless only because that line is row 0 of a text row, which is blank
; in every glyph (assets/nes/font4x8.txt) and so looks the same whichever
; table it is fetched from.
;
; The counter counts the PPU's A12 rises, one a line while it renders with
; the background at $0000 and the sprites at $1000. MesenCE and the hardware
; reload it on the pre-render line, so a latch of 8 interrupts at the end of
; line 7. The cartridge's MAME model reloads on line 0 instead; build with
; IRQ_EARLY=-1 there (README.md, "NES implementation notes").
;
; MMC3's bank select register is shared: the program sets R6 for its banks
; (prg_r6, bank_tramp), the interrupts R0/R1 for the bands and R5 for the
; body store. The program's choice is kept in mmc3_sel, written before the
; register, and whatever the interrupts change they put back -- so an
; interrupt between the program's two writes is harmless.
;
; Nothing here or in ppu.s ever writes $5500-$57FF: a FujiNet transaction in
; flight survives any number of interrupts as long as that holds.
;
        .export         __STARTUP__ : absolute = 1
        .export         _exit
        .export         _prg_r6, _bank_tramp
        .export         _frame_count, _oam
        .exportzp       _ppu_direct, _ppu_off, _mmc3_sel, _cur_bank
        .exportzp       _spin_on, _spin_y
        .import         initlib, donelib, callmain, zerobss, copydata
        .import         callptr4
        .import         _ppu_drain
        .importzp       c_sp, ptr4, tmp4
        .include        "zeropage.inc"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
OAMDMA    = $4014

CTRL_ON   = $A0                 ; NMI, 8 x 16 sprites, background at $0000
MASK_ALL  = $1E                 ; background and sprites, no left clipping
SCROLL_Y  = 8                   ; name table row 2 is the top text row

.ifndef IRQ_EARLY
IRQ_EARLY = 0
.endif
IRQ_FIRST = 8 + IRQ_EARLY

; The wait before switching the display off at line 231 (see irq): ~60
; dots from the interrupt to the loop, then 15 dots a turn, to land at about
; dot 130 of line 232 -- the middle of the safe window, so the interrupt's
; few cycles of latency jitter cannot push it out.
OFF_DELAY = 9

; ppu_drain's budgets, in its units of a stage tile, ~90 cycles (ppu.s).
; The IRQ's runs from line 232 and has to leave the sprites' DMA and show
; (~600 cycles) time to finish before line 261: 29 lines, ~3300 cycles, less
; the NMI's own ~150 that lands in the middle of it. The NMI's runs only when
; the IRQ's was over before the vertical blank began: from line 241, 20 lines.
; Both keep a margin -- running long is not slow, it is a broken frame.
IRQ_BUDGET = 24
NMI_BUDGET = 13

; The spinner: SPIN_N sprites from OAM slot SPIN_FIRST, one lifted SPIN_LIFT
; lines at a time, moving on every SPIN_RATE frames.
SPIN_FIRST = 12
SPIN_N     = 4
SPIN_LIFT  = 4
SPIN_RATE  = 8

; ---------------------------------------------------------------------------
; NES 2.0: mapper 4, 128K PRG, 8K PRG-RAM, 128K CHR-RAM. The cartridge's
; mapper engine (pico/nes/firmware/src/nesmap.c) sizes CHR-RAM from byte 11.
.segment "HEADER"
        .byte   "NES", $1A
        .byte   8               ; PRG: 8 x 16K
        .byte   0               ; CHR-ROM: none (RAM)
        .byte   $41             ; mapper 4 (low), vertical mirroring
        .byte   $08             ; NES 2.0
        .byte   0, 0            ; mapper high, submapper; ROM size high
        .byte   $07             ; PRG-RAM 64 << 7 = 8K
        .byte   $0B             ; CHR-RAM 64 << 11 = 128K
        .byte   0, 0, 0, 0

; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
_ppu_direct: .res 1
_ppu_off:    .res 1             ; the NMI has turned the display off
_mmc3_sel:   .res 1             ; the program's bank select
_cur_bank:   .res 1             ; the R6 bank mapped
_spin_on:    .res 1
_spin_y:     .res 1
irq_step:    .res 1
in_drain:    .res 1             ; the IRQ's drain is going
nmi_late:    .res 1             ; and the NMI came meanwhile
spin_t:      .res 1
spin_k:      .res 1
tr_a:        .res 1
tr_x:        .res 1

.segment "LOBSS"
_frame_count: .res 4

.segment "OAM"
_oam:        .res 256

; ---------------------------------------------------------------------------
.segment "STARTUP"

start:
        sei
        cld
        ldx     #$FF
        txs
        inx
        stx     PPUCTRL         ; NMI off
        stx     PPUMASK
        stx     $4010           ; no DMC interrupt
        lda     #$40
        sta     $4017           ; no APU frame interrupt
        stx     $E000           ; no MMC3 interrupt
        bit     PPUSTATUS
@v1:    bit     PPUSTATUS
        bpl     @v1

        ; MMC3: PRG mode 0 ($8000 R6, $A000 R7, $C000 fixed), CHR mode 0
        ; (2K R0/R1 at $0000, 1K R2-R5 at $1000)
        ldy     #0
@mm:    sty     $8000
        lda     mmc3_init,y
        sta     $8001
        iny
        cpy     #8
        bne     @mm
        lda     #$80
        sta     $A001           ; WRAM on, writable
        lda     #0
        sta     $A000           ; vertical mirroring

        ; RAM: all of it zero (the stack's page too: nothing is on it yet)
        lda     #0
        tax
@clr:   sta     $00,x
        sta     $0100,x
        sta     $0200,x
        sta     $0300,x
        sta     $0400,x
        sta     $0500,x
        sta     $0600,x
        sta     $0700,x
        inx
        bne     @clr
        lda     #6
        sta     _mmc3_sel
        sta     $8000
        lda     #0
        sta     _cur_bank       ; R6 holds bank 0 (mmc3_init)

        ; sprites all off the screen
        lda     #$F8
@ofs:   sta     _oam,x
        inx
        bne     @ofs

@v2:    bit     PPUSTATUS
        bpl     @v2

        jsr     zerobss
        jsr     copydata
        lda     #<$0800         ; the C stack: $0600-$07FF
        ldx     #>$0800
        sta     c_sp
        stx     c_sp+1
        lda     #1
        sta     _ppu_direct     ; the display stays off until scr_init is done
        sta     _ppu_off
        lda     #CTRL_ON
        sta     PPUCTRL         ; the frame clock
        cli                     ; (the scanline interrupts)
        jsr     initlib
        jsr     callmain
_exit:  jsr     donelib
        jmp     start

mmc3_init:
        .byte   0, 2, 16, 17, 18, 19    ; CHR: band 0; the sprites; R5 resting
        .byte   0, 13                   ; PRG: bank 0; R7's bank

; ---------------------------------------------------------------------------
.segment "CODE"

; prg_r6(bank): the window at $8000 gets PRG bank A.
_prg_r6:
        sta     _cur_bank
        ldx     #6
        stx     _mmc3_sel
        stx     $8000
        sta     $8001
        rts

; bank_tramp: #pragma wrapped-call's wrapper (neshw.h). cc65 leaves the
; callee's address in ptr4 and its bank in tmp4, and A/X hold a __fastcall__
; argument, so they go round the switch untouched -- and the result in A/X
; (and sreg) comes back the same way. The bank that was mapped goes on the
; hardware stack, so calls nest.
_bank_tramp:
        sta     tr_a
        stx     tr_x
        lda     _cur_bank
        pha
        lda     tmp4
        jsr     _prg_r6
        lda     tr_a
        ldx     tr_x
        jsr     callptr4
        sta     tr_a
        stx     tr_x
        pla
        jsr     _prg_r6
        lda     tr_a
        ldx     tr_x
        rts

; ---------------------------------------------------------------------------
; The sprites' DMA, then the display for the next frame: band 3's tables
; (whose fixed half holds the blank tile name table rows 0 and 1 show above
; the text), the scroll, and the scanline counter set for line 7.
oam_show:
        lda     #0
        sta     OAMADDR
        lda     #>_oam
        sta     OAMDMA
        lda     #0
        sta     $8000
        lda     #12
        sta     $8001
        lda     #1
        sta     $8000
        lda     #14
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
        lda     #0
        sta     irq_step
        bit     PPUSTATUS
        lda     #CTRL_ON
        sta     PPUCTRL
        lda     #0
        sta     PPUSCROLL
        lda     #SCROLL_Y
        sta     PPUSCROLL
        lda     #MASK_ALL
        sta     PPUMASK
        sta     $E000
        lda     #IRQ_FIRST
        sta     $C000
        sta     $C001
        sta     $E001
        rts

; ---------------------------------------------------------------------------
nmi:
        pha
        txa
        pha
        tya
        pha
        inc     _frame_count
        bne     @c
        inc     _frame_count+1
        bne     @c
        inc     _frame_count+2
        bne     @c
        inc     _frame_count+3
@c:     jsr     spin_tick
        lda     _ppu_direct
        bne     @direct
        lda     in_drain
        bne     @late
        lda     #NMI_BUDGET
        jsr     _ppu_drain
        jsr     oam_show
        jmp     @out
@late:  lda     #1              ; the IRQ's drain shows it when done
        sta     nmi_late
        jmp     @out
@direct:
        lda     #0
        sta     PPUMASK
        sta     $E000
        lda     #1
        sta     _ppu_off
@out:   pla
        tay
        pla
        tax
        pla
        rti

; The busy indicator: every SPIN_RATE frames the next of the SPIN_N sprites
; is lifted and the one before it put back, so the dots bob left to right.
spin_tick:
        lda     _spin_on
        beq     @r
        dec     spin_t
        bpl     @r
        lda     #SPIN_RATE - 1
        sta     spin_t
        ldx     spin_k
        inx
        cpx     #SPIN_N
        bcc     @k
        ldx     #0
@k:     stx     spin_k
        ldx     #0              ; the dot
        ldy     #0              ; its OAM entry
@l:     lda     _spin_y
        cpx     spin_k
        bne     @st
        sec
        sbc     #SPIN_LIFT
@st:    sta     _oam + SPIN_FIRST * 4,y
        iny
        iny
        iny
        iny
        inx
        cpx     #SPIN_N
        bne     @l
@r:     rts

; ---------------------------------------------------------------------------
irq:
        pha
        lda     irq_step
        cmp     #4
        bcs     @off
        txa
        pha
        ldx     irq_step
        lda     #0
        sta     $8000
        lda     band_r0,x
        sta     $8001
        lda     #1
        sta     $8000
        lda     band_r1,x
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
        sta     $E000           ; acknowledge
        lda     band_next,x
        sta     $C000
        sta     $C001
        sta     $E001
        inc     irq_step
        pla
        tax
        pla
        rti

        ; line 231: off, and the blank's work. Not at once, though: the
        ; interrupt arrives in the sprite fetches at the end of line 231
        ; (dots 257-320), and switching rendering off there -- or in the
        ; first 64 dots of a line -- corrupts a row of OAM that the PPU then
        ; overwrites with row 0 when rendering comes back on, after this
        ; frame's DMA has gone. So the store is held back until the middle of
        ; line 232 (blank: it shows name table row 0's blank tiles), where
        ; switching off corrupts nothing. OFF_DELAY is in 5-cycle loops.
@off:   txa
        pha
        ldx     #OFF_DELAY
@dl:    dex
        bne     @dl
        lda     #0
        sta     PPUMASK
        sta     $E000
        tya
        pha
        lda     #1
        sta     in_drain
        lda     #IRQ_BUDGET
        jsr     _ppu_drain
        lda     #0
        sta     in_drain
        lda     nmi_late
        beq     @ret
        lda     #0
        sta     nmi_late
        jsr     oam_show
@ret:   pla
        tay
        pla
        tax
        pla
        rti

band_r0:   .byte   0, 4, 8, 12
band_r1:   .byte   2, 6, 10, 14
band_next: .byte   63, 63, 63, 31

; ---------------------------------------------------------------------------
.segment "VECTORS"
        .word   nmi
        .word   start
        .word   irq

.segment "CLAIM"
        .byte   "FUJI"
