;
; The overlay loader, for the HDOS build (H89_HDOS).
;
; HDOS leaves the program's own file open on channel -1, so the overlays are
; appended to GMAIL.ABS past the part HDOS loads -- each from a sector
; boundary, in the order of src/h89/mmap.inc -- and read from there into the
; window at H89_OVL_WIN with .POSIT and .READ. That works from any disk HDOS
; can RUN from, SY1: or FN0:, without knowing which.
;
; sccz80 compiles a call to a __banked function (GM_OVL) as call banked_call,
; then a defq of the target, whose 64K digit is its overlay. banked_call
; loads that overlay if the window holds another, calls the target with the
; stack as a direct call leaves it, and on the way back loads the overlay of
; the frame it returns to -- the caller's, if the caller was an overlay and
; something displaced it; a resident caller asks for nothing.
;
; A load is disk I/O, and from FN0: it is the FujiNet card's, so a banked
; call must never come between plat_net_begin() and plat_net_end().
;

IFDEF H89_HDOS

    EXTERN  __BSS_head
    EXTERN  __OVL_FETCH_head, __OVL_FETCH_END_head
    EXTERN  __OVL_BROWSE_head, __OVL_BROWSE_END_head
    EXTERN  __OVL_COMPOSE_head, __OVL_COMPOSE_END_head
    EXTERN  __restore_sp_onexit
    EXTERN  _plat_shutdown

    PUBLIC  banked_call
    PUBLIC  _ovl_tab

    defc    POSIT = 0x27
    defc    READ = 0x04
    defc    SCOUT = 0x02
    defc    SELF = 0xFF             ; the channel HDOS loaded the program on

    ; Each overlay's first sector in the file and its length in sectors. The
    ; file is the 8-byte ABS header and the image up to BSS, padded to a
    ; sector, then the overlays, each padded to one. tools/h89/hdosovl.py
    ; packs it so and checks this table against what it packed.
    defc    IMG = (__BSS_head - H89_ORG + 8 + 255) / 256
    defc    N_FETCH = (__OVL_FETCH_END_head - __OVL_FETCH_head + 255) / 256
    defc    N_BROWSE = (__OVL_BROWSE_END_head - __OVL_BROWSE_head + 255) / 256
    defc    N_COMPOSE = (__OVL_COMPOSE_END_head - __OVL_COMPOSE_head + 255) / 256

    SECTION rodata_user
_ovl_tab:
    defw    IMG
    defb    N_FETCH
    defw    IMG + N_FETCH
    defb    N_BROWSE
    defw    IMG + N_FETCH + N_BROWSE
    defb    N_COMPOSE

    SECTION code_user

banked_call:
    pop     hl                  ; -> the defq
    ld      e,(hl)
    inc     hl
    ld      d,(hl)
    inc     hl
    ld      c,(hl)              ; the overlay
    inc     hl
    inc     hl
    push    de                  ; the target
    ex      de,hl               ; DE = where the call returns to
    ld      hl,(ovl_sp)         ; a frame: that, and the overlay it wanted
    dec     hl
    ld      a,(ovl_req)
    ld      (hl),a
    dec     hl
    ld      (hl),d
    dec     hl
    ld      (hl),e
    ld      (ovl_sp),hl
    ld      a,c
    ld      (ovl_req),a
    call    ovl_need
    pop     hl
    call    jphl                ; the target sees its arguments where it would
    push    hl                  ; the result, which may be a long
    push    de
    ld      hl,(ovl_sp)
    ld      c,(hl)
    inc     hl
    ld      b,(hl)
    inc     hl
    ld      a,(hl)
    inc     hl
    ld      (ovl_sp),hl
    ld      (ovl_req),a
    push    bc
    call    ovl_need
    pop     bc
    pop     de
    pop     hl
    push    bc
    ret
jphl:
    jp      (hl)

; The window to hold overlay A (0: whatever is there will do).
ovl_need:
    or      a
    ret     z
    ld      hl,ovl_cur
    cp      (hl)
    ret     z
    ld      (hl),0              ; neither the old one nor the new, until read
    ld      (ovl_n),a
    push    ix
    push    iy
    dec     a
    ld      e,a
    add     a,a
    add     a,e
    ld      e,a
    ld      d,0
    ld      hl,_ovl_tab
    add     hl,de
    ld      c,(hl)
    inc     hl
    ld      b,(hl)
    inc     hl
    ld      a,(hl)
    push    af
    ld      a,SELF
    rst     0x38
    defb    POSIT
    pop     de                  ; D = sectors
    jr      c,ovl_fail
    ld      b,d
    ld      c,0
    ld      de,H89_OVL_WIN
    ld      a,SELF
    rst     0x38
    defb    READ
    jr      c,ovl_fail
    pop     iy
    pop     ix
    ld      a,(ovl_n)
    ld      (ovl_cur),a
    ret

; Nothing can go on without the code, so say which and leave.
ovl_fail:
    push    af
    call    _plat_shutdown
    ld      hl,ovl_msg1
    call    ovl_puts
    ld      a,(ovl_n)
    call    ovl_dec
    ld      hl,ovl_msg2
    call    ovl_puts
    pop     af
    call    ovl_dec
    ld      a,10
    rst     0x38
    defb    SCOUT
    jp      __restore_sp_onexit

ovl_puts:
    ld      a,(hl)
    or      a
    ret     z
    push    hl
    rst     0x38
    defb    SCOUT
    pop     hl
    inc     hl
    jr      ovl_puts

; A, 0-99, in decimal.
ovl_dec:
    ld      b,'0' - 1
ovl_tens:
    inc     b
    sub     10
    jr      nc,ovl_tens
    add     a,'0' + 10
    push    af
    ld      a,b
    rst     0x38
    defb    SCOUT
    pop     af
    rst     0x38
    defb    SCOUT
    ret

    SECTION rodata_user
ovl_msg1:
    defm    "GMAIL: overlay ", 0
ovl_msg2:
    defm    " did not load, HDOS error ", 0

    SECTION data_user
ovl_sp:
    defw    ovl_stk_end

    SECTION bss_user
ovl_cur:
    defb    0                   ; the overlay in the window
ovl_req:
    defb    0                   ; the overlay the innermost banked call wants
ovl_n:
    defb    0
ovl_stk:
    defs    3 * 8               ; banked calls nest two deep here
ovl_stk_end:

ENDIF
