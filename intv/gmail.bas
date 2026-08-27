' gmail.bas -- FujiNet Gmail inbox reader for the Intellivision.
'
' Reads mail from the FujiNet GMAIL protocol adapter (N:GMAIL:) over the
' $9C00 memory-mapped mailbox. Two screens: an INBOX list with a
' color-stack selection bar that bounce-scrolls the highlighted line, and a
' MESSAGE reader with a wrapped, scrollable body.
'
' Authorization is the FujiNet's, not this program's: the adapter reuses
' the Google Drive OAuth grant (widened to include gmail.readonly), so the
' user authorizes once in the FujiNet Web UI and this client never sees a
' credential. A 212 from any open means that has not been done.
'
' See README.md for controls and known limitations, and constants.bas for
' the memory map, wire layout and the two rules that govern every line here
' (all tabular data in SC_* cart RAM; every PEEK into a #-prefixed
' destination).
'
' ---------------------------------------------------------------------------
' The GOTO must jump clear of every INCLUDE. IntyBASIC pastes them in
' verbatim, and straight-line execution falling into a PROCEDURE body or a
' DATA block corrupts the return stack.
' ---------------------------------------------------------------------------
    GOTO gm_start

    INCLUDE "constants.bas"

' ===========================================================================
' Shared state.
'
' These are DIMed here, ahead of every INCLUDE that touches them, because
' IntyBASIC auto-creates a variable on first USE and then rejects a later
' explicit DIM of the same name as a redefinition. Declaring them after the
' INCLUDEs fails to compile.
' ===========================================================================
    ' Index window.
    DIM gm_count          ' entries actually parsed into SC_IDX (0..IDX_MAX)
    DIM #gm_range         ' absolute 0-based index of the first entry fetched
    DIM #gm_total_lo, #gm_total_hi   ' folder message count, 32-bit
    DIM gm_next           ' 1 if a further range exists past this window
    DIM gm_list_valid     ' 1 once SC_IDX holds a usable listing

    ' Selection.
    DIM gm_vpage          ' visible 8-row page within the fetched 16 (0 or 1)
    DIM gm_slot           ' highlighted row within the visible page (0..7)
    DIM gm_vis            ' entries drawn on the current visible page
    DIM gm_sel, gm_e      ' absolute entry index (0..gm_count-1)

    ' Status/error reporting.
    DIM gm_busy           ' 1 = opening the mailbox, 2 = fetching a message
    DIM #gm_ecode

    ' fmt_u32 / string helpers.
    DIM #fm_lo, #fm_hi, #fm_r, #fm_t, #fm_p
    DIM fm_i
    DIM #cs_src, #cs_dst, #cs_c
    DIM cs_max, cs_o, cs_i, cs_hb

    INCLUDE "fujinet.bas"
    INCLUDE "input.bas"
    INCLUDE "screen.bas"
    INCLUDE "scroll.bas"
    INCLUDE "wrap.bas"
    INCLUDE "gfx.bas"
    INCLUDE "st_inbox.bas"
    INCLUDE "st_msg.bas"

' ===========================================================================
' fmt_u32 -- 32-bit unsigned to decimal ASCII, using only 16-bit arithmetic.
'
' In:  #fm_hi/#fm_lo (value = #fm_hi*65536 + #fm_lo)
' Out: #fm_p -> first digit of a NUL-terminated string in SC_NUM32.
'      #fm_hi/#fm_lo are consumed.
'
' fujinet.bas's fn_putnum caps at 999, which is nowhere near enough: msgNum
' is a position within the folder and reaches the mailbox's total message
' count, and the same routine renders the "1-16/1234" page indicator.
'
' The division identity, since it is not obvious. Write V = hi*65536 + lo
' and peel the high word first: hi = 10*hq + hr. Then
'
'   V = 10*(hq*65536) + (hr*65536 + lo)
'
' and since 65536 = 10*6553 + 6,
'
'   hr*65536 + lo = 10*(hr*6553) + 6*hr + lo
'                 = 10*(hr*6553) + 10*(lo/10) + t,  t = 6*hr + lo%10
'                 = 10*(hr*6553 + lo/10 + t/10) + t%10
'
' so the digit is t%10 and the quotient's words are hq and
' (hr*6553 + lo/10 + t/10). Every intermediate fits in 16 bits: t <= 63,
' and the new low word peaks at 65535 (hr=9 forces lo%10 <= 5 whenever
' lo/10 is at its 6553 maximum, so t/10 can only reach 5 there, never 6).
'
' IntyBASIC's / and % are unsigned on 16-bit values (manual.txt:385), which
' is what makes this safe at all -- a signed divide would wrap the moment a
' word passed 32767. All the operands are #-prefixed to force 16-bit
' arithmetic; an 8-bit intermediate would silently truncate hr*6553.
' ===========================================================================
fmt_u32: PROCEDURE
    fm_i = 11
    POKE SC_NUM32 + 11, 0
fu_digit:
    #fm_r = #fm_hi % 10
    #fm_t = #fm_r * 6 + (#fm_lo % 10)
    #fm_lo = #fm_r * 6553 + (#fm_lo / 10) + (#fm_t / 10)
    #fm_hi = #fm_hi / 10
    fm_i = fm_i - 1
    POKE SC_NUM32 + fm_i, (#fm_t % 10) + 48
    IF (#fm_hi <> 0) OR (#fm_lo <> 0) THEN GOTO fu_digit
    #fm_p = SC_NUM32 + fm_i
END

' ===========================================================================
' gm_copy_san -- copy a NUL-terminated field, sanitizing as it goes.
'
' In:  #cs_src source, #cs_dst destination, cs_max max characters copied.
' Out: destination NUL-terminated, cs_o = characters written.
'
' The adapter does no charset work at all: subjects and sender names are
' raw UTF-8 straight out of the Gmail API, with no RFC 2047 decoding
' either. On an 8-bit console the only honest thing to do with a byte above
' 126 is substitute it -- and to collapse a RUN of them into a single '?',
' so one accented character costs one placeholder instead of two or three.
' ===========================================================================
gm_copy_san: PROCEDURE
    cs_o = 0
    cs_i = 0
    cs_hb = 0
cs_loop:
    IF cs_i >= cs_max THEN GOTO cs_done
    #cs_c = PEEK(#cs_src + cs_i) AND 255
    IF #cs_c = 0 THEN GOTO cs_done
    cs_i = cs_i + 1
    IF #cs_c > 126 THEN
        IF cs_hb = 1 THEN GOTO cs_loop
        cs_hb = 1
        #cs_c = 63                      ' '?'
    ELSE
        cs_hb = 0
        IF #cs_c < 32 THEN #cs_c = 32
    END IF
    POKE (#cs_dst + cs_o), #cs_c
    cs_o = cs_o + 1
    GOTO cs_loop
cs_done:
    POKE (#cs_dst + cs_o), 0
END

' ===========================================================================
' Transient screens
' ===========================================================================

' gm_video_box: all-black color stack for the status and error screens.
'
' The WAIT is load-bearing. MODE packs its four colors into IntyBASIC's
' internal _color variable and flags _mode_select; the ISR consumes that on
' the next frame and resets _color to 7. Any PRINT ... COLOR in between
' would overwrite the packed word and bring the stack up as garbage. The
' invariant order is MODE : BORDER : WAIT : CLS, everywhere.
gm_video_box: PROCEDURE
    MODE 0, CS_BLACK, CS_BLACK, CS_BLACK, CS_BLACK
    BORDER CS_BLACK
    WAIT
    CLS
END

' gm_fetching: put a status message up BEFORE a blocking transact, not
' after. A GMAIL open holds the CPU inside fn_transact's poll loop for as
' long as sixty seconds; whatever is on screen when it starts is what the
' user looks at for the whole wait, so it has to say what is happening and
' roughly how long. gm_busy picks the wording.
'   1 = opening the mailbox (the slow one -- one HTTPS round trip per entry)
'   2 = fetching a message body
gm_fetching: PROCEDURE
    GOSUB gm_logo_hide
    GOSUB gm_video_box
    IF gm_busy = 1 THEN
        PRINT AT screenpos(1, 4) COLOR COL_BOX, "Opening mailbox..."
        PRINT AT screenpos(2, 6) COLOR COL_MSUBJ, "up to 60 seconds"
    ELSE
        PRINT AT screenpos(1, 4) COLOR COL_BOX, "Fetching message..."
    END IF
END

' gm_map_err: turn a failed open into #gm_ecode.
'
' fn_ok = 0 with #mb_err = 0 is fn_transact's own timeout -- the peripheral
' never answered at all, so there is no protocol error to report. Otherwise
' the device layer saved the protocol's error before tearing it down
' (rs232/network.cpp), and a STATUS with no protocol open hands it back as
' byte 3, which is exactly what net_status exposes as #net_err.
gm_map_err: PROCEDURE
    IF #mb_err = 0 THEN
        #gm_ecode = 0
        RETURN
    END IF
    GOSUB net_status
    #gm_ecode = #net_err
END

' gm_error: show #gm_ecode and wait for any key.
gm_error: PROCEDURE
    GOSUB gm_logo_hide
    GOSUB gm_video_box
    PRINT AT screenpos(4, 1) COLOR COL_BOXERR, "Gmail error"

    IF #gm_ecode = ERR_NOAUTH THEN
        PRINT AT screenpos(0, 4) COLOR COL_BOX, "Authorize Google in"
        PRINT AT screenpos(0, 5) COLOR COL_BOX, "the FujiNet Web UI"
    ELSEIF #gm_ecode = ERR_DENIED THEN
        PRINT AT screenpos(0, 4) COLOR COL_BOX, "Google access denied"
        PRINT AT screenpos(0, 5) COLOR COL_BOX, "re-authorize (scope)"
    ELSEIF #gm_ecode = ERR_NOTFOUND THEN
        PRINT AT screenpos(0, 4) COLOR COL_BOX, "Message not found"
    ELSEIF #gm_ecode = ERR_NOSERVICE THEN
        PRINT AT screenpos(0, 4) COLOR COL_BOX, "Service unavailable"
        PRINT AT screenpos(0, 5) COLOR COL_BOX, "check connection"
    ELSEIF #gm_ecode = 0 THEN
        PRINT AT screenpos(0, 4) COLOR COL_BOX, "No reply from"
        PRINT AT screenpos(0, 5) COLOR COL_BOX, "FujiNet (timeout)"
    ELSE
        PRINT AT screenpos(0, 4) COLOR COL_BOX, "Error ", <>#gm_ecode
    END IF

    PRINT AT screenpos(4, 9) COLOR COL_MSUBJ, "PRESS ANY KEY"
    GOSUB in_anykey
END

' ===========================================================================
' High-water mark -- the read/unread model.
'
' The GMAIL adapter exposes no read/unread state. Its only per-message flag
' is IMPORTANT, and that one exists solely in the human-readable listing
' format; the raw 220-byte MailIndexItem this client parses has no flags
' byte at all (Mailbox.cpp format_index_raw). Gmail's own UNREAD label is
' sitting right there in the labelIds array the firmware parses and is
' simply never looked at.
'
' So "unread" here is local and inferred: one 8-byte timestamp persisted in
' a FujiNet appkey, and any message newer than it is shown as unread.
' Reading a message advances the mark to that message's timestamp, which
' marks it AND everything older as read. That is inherent to a high-water
' mark, it is documented in the README, and it matches how one actually
' reads down an inbox.
'
' Creator $474D is "GM" little-endian.
' ===========================================================================
DIM hw_i, hw_gt
DIM #hw_addr, #hw_a, #hw_b

hwm_ids: PROCEDURE
    ak_creator_lo = $4D : ak_creator_hi = $47
    ak_app = 1 : ak_key = 0
END

hwm_load: PROCEDURE
    GOSUB hwm_ids
    ak_mode = 0
    GOSUB appkey_open
    IF fn_ok = 0 THEN GOTO hl_zero
    ' ls_max 9, not 8: appkey_read always writes a NUL at #fn_src + fn_len,
    ' so an 8-byte payload touches SC_HWM+8. That byte is inside the 16
    ' reserved for SC_HWM and belongs to nothing else.
    #fn_src = SC_HWM : ls_max = 9
    GOSUB appkey_read
    GOSUB appkey_close
    IF fn_len >= 8 THEN RETURN
hl_zero:
    ' No stored mark (first run) or an unreadable one: zero it, which makes
    ' every message newer and so the whole inbox shows as unread.
    FOR hw_i = 0 TO 7
        POKE SC_HWM + hw_i, 0
    NEXT hw_i
END

hwm_save: PROCEDURE
    GOSUB hwm_ids
    ak_mode = 1
    GOSUB appkey_open
    IF fn_ok = 0 THEN RETURN
    #fn_src = SC_HWM : fn_len = 8
    GOSUB appkey_write
    GOSUB appkey_close
END

' hwm_newer: is the 8-byte little-endian timestamp at #hw_addr strictly
' greater than SC_HWM? Sets hw_gt. Compared from the most significant byte
' down; equal means NOT newer, so the message just read counts as read.
'
' The loop counts down without STEP -1 on purpose: hw_i is an unsigned
' 8-bit variable and a FOR that has to notice it went below zero is exactly
' the shape that misbehaves.
hwm_newer: PROCEDURE
    hw_gt = 0
    hw_i = 8
hn_loop:
    hw_i = hw_i - 1
    #hw_a = PEEK(#hw_addr + hw_i) AND 255
    #hw_b = PEEK(SC_HWM + hw_i) AND 255
    IF #hw_a > #hw_b THEN hw_gt = 1 : RETURN
    IF #hw_a < #hw_b THEN RETURN
    IF hw_i > 0 THEN GOTO hn_loop
END

' hwm_update: called after a message body has been fetched. Advances the
' mark if that message is newer than it, and persists it.
hwm_update: PROCEDURE
    #hw_addr = SC_IDX + gm_sel * IDX_STRIDE + IDX_TS
    GOSUB hwm_newer
    IF hw_gt = 0 THEN RETURN
    FOR hw_i = 0 TO 7
        POKE SC_HWM + hw_i, PEEK(#hw_addr + hw_i) AND 255
    NEXT hw_i
    GOSUB hwm_save
END

' hwm_flags: recompute every cached entry's unread bit against the current
' mark. Cheap (16 entries x an 8-byte compare) and has to run after any
' change to SC_HWM, since the mark moving turns entries read in bulk.
hwm_flags: PROCEDURE
    IF gm_count = 0 THEN RETURN
    fm_i = 0
hf_loop:
    #hw_addr = SC_IDX + fm_i * IDX_STRIDE + IDX_TS
    GOSUB hwm_newer
    POKE (SC_IDX + fm_i * IDX_STRIDE + IDX_FLAGS), hw_gt
    fm_i = fm_i + 1
    IF fm_i < gm_count THEN GOTO hf_loop
END

' ===========================================================================
' ROM literals.
'
' Raw ASCII bytes, NUL-terminated. DATA "strings" store CARD codes, not
' ASCII, and these go on the wire as a devicespec -- they have to be true
' ASCII. Generated from the quoted strings in the comments.
' ===========================================================================
    ' "N:GMAIL:///INBOX?range="
lit_idx_base:
    DATA 78,58,71,77,65,73,76,58,47,47,47,73,78,66,79,88
    DATA 63,114,97,110,103,101,61,0

    ' "N:GMAIL:///INBOX/"
lit_body_base:
    DATA 78,58,71,77,65,73,76,58,47,47,47,73,78,66,79,88,47,0

    ' "-"
lit_dash:
    DATA 45,0

' ===========================================================================
' Boot
' ===========================================================================
gm_start:
    ' Cart RAM and IntyBASIC variables both come up as garbage, and
    ' fn_transact consults #fn_tmo on its very first call, so this has to
    ' be the first thing set.
    #fn_tmo = TMO_NORM

    GOSUB gm_define_gram
    GOSUB gm_video_box

gm_wait_fn:
    PRINT AT screenpos(3, 4) COLOR COL_BOX, "FujiNet Gmail"
    PRINT AT screenpos(1, 6) COLOR COL_MSUBJ, "Waiting for FujiNet"
    GOSUB fn_wait_mailbox
    IF fn_ok = 1 THEN GOTO gm_have_fn

    GOSUB gm_video_box
    PRINT AT screenpos(2, 4) COLOR COL_BOXERR, "FujiNet not found"
    PRINT AT screenpos(2, 6) COLOR COL_BOX, "Check the adapter"
    PRINT AT screenpos(3, 9) COLOR COL_MSUBJ, "ANY KEY: RETRY"
    GOSUB in_anykey
    GOSUB gm_video_box
    GOTO gm_wait_fn

gm_have_fn:
    GOSUB hwm_load

    #gm_range = 0
    gm_vpage = 0
    gm_slot = 0
    gm_list_valid = 0

    GOTO inbox_enter
