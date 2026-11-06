;
; ppu.s -- the text screen's engine: the composer, which turns a row of the
; text screen into tile pixels, and the drain, which copies to the PPU
; whatever has changed (src/nes/neshw.h).
;
; The program never touches the PPU while the screen is on. It writes
; characters into scr_txt and marks the tiles that changed (scr.c); the
; composer, run by the program between frames, renders the first dirty row
; into the stage; and the drain, run in the blank by the interrupts (crt0.s),
; copies the stage across. A row is 32 tiles, two characters to a tile, and
; only rows 1-6 of a tile are ever written -- 0 and 7 are blank in every
; glyph and stay as scr_init left them.
;
; The drain also carries the palette, the attribute table and the body
; store's rows (bodystore.c), in that order, as far as its budget goes. A
; unit of budget is one tile of the stage, about 90 cycles; the other jobs
; are charged in tiles' worth. Getting these costs wrong is not a matter of
; speed: a drain that runs past the end of the vertical blank leaves show
; (crt0.s) switching the screen on mid-frame, and that frame -- and the
; VRAM under it -- come out wrong.
;
        .export         _scr_compose, _ppu_drain, _ppu_busy
        .export         _scr_txt, _dirty_lo, _dirty_hi
        .export         _att, _att_dirty, _pal, _pal_dirty
        .export         _bs_wbuf, _bs_wbank, _bs_whi, _bs_wlo
        .export         _bs_head, _bs_tail
        .export         _bs_rd_buf, _bs_rd_bank, _bs_rd_hi, _bs_rd_lo
        .export         _bs_rd_state
        .importzp       _mmc3_sel
        .import         _font_l1, _font_l2, _font_l3, _font_l4, _font_l5, _font_l6
        .import         _font_r1, _font_r2, _font_r3, _font_r4, _font_r5, _font_r6

PPUSTATUS = $2002
PPUADDR   = $2006
PPUDATA   = $2007

ROWS      = 28
BS_RING   = 4

; units: one is a tile of the stage, ~90 cycles
C_PAL     = 6                   ; 32 bytes and the address twice: ~480
C_ATT     = 2                   ; 8 bytes: ~150
C_ROW     = 7                   ; a body store row, either way: ~600

; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
; the drain's (interrupt context, or the program's with the screen off)
d_budget:  .res 1
d_cost:    .res 1
d_k:       .res 1
; the composer's (the program's)
c_src:     .res 2
c_tlo:     .res 1
c_n:       .res 1
c_si:      .res 1
c_x:       .res 1
c_c1:      .res 1
c_t:       .res 6

.segment "STAGE"
st_state:  .res 1               ; 0 free, 1 waiting for the drain
st_band:   .res 1               ; which band's tables to map
st_hi:     .res 1               ; the next tile's row 1, in the band's table
st_lo:     .res 1
st_n:      .res 1               ; tiles left
st_x:      .res 1               ; their bytes start here in st_data
st_data:   .res 32 * 6

.segment "LOBSS"
_att:         .res 64
_pal:         .res 32
_dirty_lo:    .res ROWS
_dirty_hi:    .res ROWS
_att_dirty:   .res 1
_pal_dirty:   .res 1
_bs_wbank:    .res BS_RING
_bs_whi:      .res BS_RING
_bs_wlo:      .res BS_RING
_bs_head:     .res 1
_bs_tail:     .res 1
_bs_rd_bank:  .res 1
_bs_rd_hi:    .res 1
_bs_rd_lo:    .res 1
_bs_rd_state: .res 1

.segment "RING"
_bs_wbuf:     .res BS_RING * 64

.segment "BSS"
_scr_txt:     .res ROWS * 64
_bs_rd_buf:   .res 64

; ---------------------------------------------------------------------------
.segment "CODE"

chr_r0:   .byte   0, 4, 8, 12
chr_r1:   .byte   2, 6, 10, 14
bit_of:   .byte   $01, $02, $04, $08, $10, $20, $40, $80

; take: spend A units of the budget. C set if there were enough (always, with
; a budget of 255); C clear and nothing spent if not. X and Y untouched.
take:
        sta     d_cost
        lda     d_budget
        cmp     #255
        beq     @yes
        sec
        sbc     d_cost
        bcc     @no
        sta     d_budget
@yes:   sec
        rts
@no:    clc
        rts

; ppu_drain(budget)
_ppu_drain:
        sta     d_budget
        bit     PPUSTATUS       ; the address latch: first write next

        ; --- the palette
        lda     _pal_dirty
        beq     @att
        lda     #C_PAL
        jsr     take
        bcs     @pal
        rts
@pal:   lda     #0
        sta     _pal_dirty
        lda     #$3F
        sta     PPUADDR
        lda     #$00
        sta     PPUADDR
        ldx     #0
@pl:    lda     _pal,x
        sta     PPUDATA
        inx
        cpx     #32
        bne     @pl
        lda     #0              ; off the palette: with rendering off, the
        sta     PPUADDR         ; PPU shows the colour v points at
        sta     PPUADDR

        ; --- the attribute table, a row of eight bytes for each dirty bit
@att:   lda     _att_dirty
        beq     @ring
        ldy     #0
@ak:    lda     _att_dirty
        and     bit_of,y
        beq     @an
        lda     #C_ATT
        jsr     take
        bcs     @aw
        rts
@aw:    lda     bit_of,y
        eor     #$FF
        and     _att_dirty
        sta     _att_dirty
        lda     #$23
        sta     PPUADDR
        tya
        asl     a
        asl     a
        asl     a
        tax
        ora     #$C0
        sta     PPUADDR
        .repeat 8, I
        lda     _att + I,x
        sta     PPUDATA
        .endrepeat
@an:    iny
        cpy     #8
        bne     @ak

        ; --- the body store's writes: R5 gets the row's bank, and the row
        ; goes to its place in the window at $1C00
@ring:  ldx     _bs_tail
        cpx     _bs_head
        bne     @rt
        jmp     @read
@rt:    lda     #C_ROW
        jsr     take
        bcs     @rw
        rts
@rw:    lda     #5
        sta     $8000
        lda     _bs_wbank,x
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
        lda     _bs_whi,x
        sta     PPUADDR
        lda     _bs_wlo,x
        sta     PPUADDR
        txa                     ; Y = the slot's first byte in bs_wbuf
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        tay
        lda     #4
        sta     d_k
@wq:    .repeat 16, I
        lda     _bs_wbuf + I,y
        sta     PPUDATA
        .endrepeat
        tya
        clc
        adc     #16
        tay
        dec     d_k
        beq     @wd
        jmp     @wq
@wd:    inx
        txa
        and     #BS_RING - 1
        sta     _bs_tail
        jmp     @ring

        ; --- a body store read: the first read after setting the address
        ; is the PPU's stale buffer, so it is thrown away
@read:  lda     _bs_rd_state
        cmp     #1
        beq     @rq
        jmp     @stage
@rq:    lda     #C_ROW
        jsr     take
        bcs     @rr
        rts
@rr:    lda     #5
        sta     $8000
        lda     _bs_rd_bank
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
        lda     _bs_rd_hi
        sta     PPUADDR
        lda     _bs_rd_lo
        sta     PPUADDR
        lda     PPUDATA
        .repeat 64, I
        lda     PPUDATA
        sta     _bs_rd_buf + I
        .endrepeat
        lda     #2
        sta     _bs_rd_state

        ; --- the stage, a tile at a time, for as long as the budget lasts:
        ; what is left is still there for the next drain
@stage: lda     st_state
        bne     @band
        rts
@band:  ldx     st_band
        lda     #0
        sta     $8000
        lda     chr_r0,x
        sta     $8001
        lda     #1
        sta     $8000
        lda     chr_r1,x
        sta     $8001
        lda     _mmc3_sel
        sta     $8000

        ; this time: min(tiles left, budget) -- every tile is one unit
        lda     d_budget
        cmp     #255
        beq     @all
        cmp     st_n
        bcc     @some
@all:   lda     st_n
@some:  sta     d_k
        bne     @go
        rts
@go:    ldx     st_x
@tl:    lda     st_hi
        sta     PPUADDR
        lda     st_lo
        sta     PPUADDR
        .repeat 6, I
        lda     st_data + I,x
        sta     PPUDATA
        .endrepeat
        txa
        clc
        adc     #6
        tax
        lda     st_lo
        clc
        adc     #16
        sta     st_lo
        bcc     @tn
        inc     st_hi
@tn:    dec     st_n
        dec     d_k
        bne     @tl
        stx     st_x
        lda     st_n
        bne     @sr
        sta     st_state
@sr:    rts

; ppu_busy(): anything for the drain still to do?
_ppu_busy:
        lda     _pal_dirty
        ora     _att_dirty
        ora     st_state
        bne     @yes
        lda     _bs_head
        cmp     _bs_tail
        bne     @yes
        lda     _bs_rd_state
        cmp     #1
        beq     @yes
        lda     #0
        tax
        rts
@yes:   lda     #1
        ldx     #0
        rts

; ---------------------------------------------------------------------------
; scr_compose(): render the first dirty row's dirty tiles into the stage.
; Returns 1 if it did, 0 if the stage was busy or nothing was dirty.
_scr_compose:
        lda     st_state
        beq     @free
@none:  lda     #0
        tax
        rts
@free:  ldx     #0
@find:  lda     _dirty_lo,x
        cmp     #$FF
        bne     @got
        inx
        cpx     #ROWS
        bne     @find
        beq     @none

@got:   sta     c_tlo
        lda     _dirty_hi,x
        sec
        sbc     c_tlo
        clc
        adc     #1
        sta     c_n
        sta     st_n
        lda     #$FF
        sta     _dirty_lo,x
        lda     #0
        sta     _dirty_hi,x

        ; the band: rows 0-7, 8-15, 16-23, 24-27
        txa
        lsr     a
        lsr     a
        lsr     a
        sta     st_band

        ; the first tile's row 1 in its band's table: tile (row & 7) * 32
        ; + tlo, 16 bytes a tile
        txa
        and     #7
        asl     a
        sta     c_t
        lda     c_tlo
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        clc
        adc     c_t
        sta     st_hi
        lda     c_tlo
        asl     a
        asl     a
        asl     a
        asl     a
        ora     #1
        sta     st_lo

        ; the source: scr_txt + row * 64 + tlo * 2
        stx     c_src
        lda     #0
        sta     c_src+1
        .repeat 6
        asl     c_src
        rol     c_src+1
        .endrepeat
        lda     c_tlo
        asl     a
        clc
        adc     c_src
        sta     c_src
        lda     c_src+1
        adc     #0
        sta     c_src+1
        lda     c_src
        clc
        adc     #<_scr_txt
        sta     c_src
        lda     c_src+1
        adc     #>_scr_txt
        sta     c_src+1

        lda     #0
        sta     c_x
        tay
@t:     lda     (c_src),y
        sta     c_c1
        iny
        lda     (c_src),y
        iny
        sty     c_si
        tay
        ldx     c_c1
        lda     _font_l1,x
        ora     _font_r1,y
        sta     c_t
        lda     _font_l2,x
        ora     _font_r2,y
        sta     c_t+1
        lda     _font_l3,x
        ora     _font_r3,y
        sta     c_t+2
        lda     _font_l4,x
        ora     _font_r4,y
        sta     c_t+3
        lda     _font_l5,x
        ora     _font_r5,y
        sta     c_t+4
        lda     _font_l6,x
        ora     _font_r6,y
        sta     c_t+5
        ldx     c_x
        .repeat 6, I
        lda     c_t + I
        sta     st_data + I,x
        .endrepeat
        txa
        clc
        adc     #6
        sta     c_x
        ldy     c_si
        dec     c_n
        bne     @t

        lda     #0
        sta     st_x
        lda     #1
        sta     st_state        ; last: the drain may take it from here
        ldx     #0
        rts
