;
; The HDOS build's memory map (-pragma-define:MMAP=-1): z88dk's own, then
; the three overlays. Each overlay is linked at the window, H89_OVL_WIN, with
; its number in the 64K digit, so the linker keeps them apart and a banked
; call's defq carries the number to banked_call (src/h89/ovl.asm). z80asm
; writes each one out as gmail_OVL_<name>.bin.
;

    INCLUDE "crt/classic/crt_section_standard.inc"

    SECTION OVL_FETCH
    org     $010000 + H89_OVL_WIN
    SECTION code_ovl_fetch
    SECTION rodata_ovl_fetch
    SECTION OVL_FETCH_END

    SECTION OVL_BROWSE
    org     $020000 + H89_OVL_WIN
    SECTION code_ovl_browse
    SECTION rodata_ovl_browse
    SECTION OVL_BROWSE_END

    SECTION OVL_COMPOSE
    org     $030000 + H89_OVL_WIN
    SECTION code_ovl_compose
    SECTION rodata_ovl_compose
    SECTION OVL_COMPOSE_END
