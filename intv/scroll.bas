' scroll.bas -- bounce-scroll the highlighted list row in place, so a
' "Sender: Subject" longer than the 19 visible columns can be read in full
' without opening the message.
'
' Ported from fujinet-lobby/intv/scroll.bas (itself from
' fujinet-config/intv/scroll.bas). The only change is the hook name:
' lb_load_roomname -> gm_load_entry. Text lives in SC_ENTRY and is PEEKed
' in place -- nothing is ever held in an IntyBASIC string.
'
' Driven by st_inbox.bas's inbox loop: set sc_row/sc_col/sc_max/sc_color
' and GOSUB scroll_step once per frame while a row is selected; GOSUB
' scroll_reset (with sc_row/sc_col/sc_max/sc_color describing the row being
' LEFT) whenever the cursor moves off it. st_inbox.bas provides
' gm_load_entry, which recomposes the selected entry's full text out of the
' already-cached SC_IDX into SC_ENTRY and sets sc_len -- no mailbox round
' trip, unlike config's directory browser (where the equivalent hook
' re-fetches from the device because nothing is cached there).

    CONST SCROLL_TICKS = 6      ' frames per character step (~10 chars/sec)
    CONST SCROLL_HOLD  = 30     ' frames paused at each end
    CONST IDLE_FRAMES  = 45     ' frames of no input before scrolling starts

    DIM sc_row, sc_col, sc_max, sc_color
    DIM sc_off, sc_len, sc_dir, sc_tick, sc_idle, sc_active
    DIM #sc_hold

' ---------------------------------------------------------------------------
' scroll_draw: paint sc_max cells of SC_ENTRY, starting at byte sc_off,
' onto row sc_row/column sc_col, in color sc_color, space-padded.
' ---------------------------------------------------------------------------
scroll_draw: PROCEDURE
    FOR s_i = 0 TO sc_max - 1
        #s_c = 32
        IF sc_off + s_i < sc_len THEN #s_c = PEEK(SC_ENTRY + sc_off + s_i) AND 255
        IF #s_c < 32 OR #s_c > 126 THEN #s_c = 32
        #s_val = (#s_c - 32) * 8 + sc_color
        ' The cell at sc_col carries the color stack's selection-bar advance
        ' (st_inbox.bas's gm_bar_set), and this loop runs during active
        ' display, not just in vblank. Re-arming bit 13 in a second pass
        ' after the loop leaves it clear for a dozen cell writes, and any
        ' frame the STIC scans inside that window renders the whole screen a
        ' stack position out -- blue bar, and every row below it shifted.
        ' That reads as violent flashing while a long subject pans. Folding
        ' the bit into the same word keeps every write atomic, so the bit is
        ' never observably absent.
        '
        ' This module only ever draws the selected row, so the bit always
        ' belongs here; the one call on a row being LEFT comes from
        ' scroll_reset, and gm_move_up/gm_move_down clear it immediately
        ' after.
        IF s_i = 0 THEN #s_val = #s_val + CS_ADVANCE
        #BACKTAB(sc_row * SCREEN_COLS + sc_col + s_i) = #s_val
    NEXT s_i
END

' ---------------------------------------------------------------------------
' scroll_step: call once per frame while sc_row is the highlighted row.
' After IDLE_FRAMES of no scroll_reset, recomposes the full entry text and
' starts bouncing a sc_max-wide window back and forth across it. A no-op if
' the text turns out to fit within sc_max already.
' ---------------------------------------------------------------------------
scroll_step: PROCEDURE
    IF sc_active = 0 THEN
        sc_idle = sc_idle + 1
        IF sc_idle < IDLE_FRAMES THEN RETURN
        GOSUB gm_load_entry
        sc_active = 1 : sc_off = 0 : sc_dir = 0 : sc_tick = 0 : #sc_hold = SCROLL_HOLD
        RETURN
    END IF
    IF sc_len <= sc_max THEN RETURN

    IF #sc_hold > 0 THEN
        #sc_hold = #sc_hold - 1
        RETURN
    END IF
    sc_tick = sc_tick + 1
    IF sc_tick < SCROLL_TICKS THEN RETURN
    sc_tick = 0

    IF sc_dir = 0 THEN
        IF sc_off < sc_len - sc_max THEN
            sc_off = sc_off + 1
        ELSE
            sc_dir = 1 : #sc_hold = SCROLL_HOLD
        END IF
    ELSE
        IF sc_off > 0 THEN
            sc_off = sc_off - 1
        ELSE
            sc_dir = 0 : #sc_hold = SCROLL_HOLD
        END IF
    END IF
    GOSUB scroll_draw
END

' ---------------------------------------------------------------------------
' scroll_reset: call when the cursor is about to leave row sc_row (with
' sc_color set to whatever color that row should end up in). If a scroll was
' in progress, SC_ENTRY still holds the full text, so the truncated
' (offset-0) view can be repainted from it directly. Always resets the idle
' countdown so the module is primed to start counting for whatever row the
' caller points sc_row/sc_col/sc_max at next.
' ---------------------------------------------------------------------------
scroll_reset: PROCEDURE
    IF sc_active = 1 THEN
        sc_off = 0
        GOSUB scroll_draw
    END IF
    sc_active = 0 : sc_idle = 0 : sc_dir = 0 : sc_tick = 0 : #sc_hold = 0
END
