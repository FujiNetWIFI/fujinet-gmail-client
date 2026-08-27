' st_inbox.bas -- the INBOX screen: fetch, parse, render, selection bar,
' paging.
'
' ---------------------------------------------------------------------------
' COLOR STACK STRUCTURE (the reason this screen is laid out the way it is)
'
' MODE 0, CS_WHITE, CS_WHITE, CS_BLUE, CS_WHITE gives four stack registers,
' and exactly four advance bits walk the screen through them:
'
'     p0  rows 0-1                    header (white)
'   advance at (0, LIST_START_ROW)
'     p1  rows 2..bar-1, plus column 0 of the bar row   (white)
'   advance at (1, bar row)
'     p2  bar row, columns 1-19       the selection bar (blue)
'   advance at (0, bar row + 1)
'     p3  rows bar+1..10              (white)
'   advance at (0, LEGEND_ROW)
'     p0  row 11 (wrapped)            legend (white)
'
' That only holds while BOTH white runs are non-empty, which is where the
' two layout rules come from: the bar spans columns 1-19 only (column 0 is
' the run that survives when the TOP entry is selected, and is also where
' the read/unread icon lives), and row 10 is a permanent blank spacer (the
' run that survives when the BOTTOM entry is selected).
'
' Because p0/p1/p3 are all white, a miscounted advance is invisible on the
' list itself -- but it would still move the BAR, so the count matters as
' much as the positions. gm_apply_stack keeps it at four even with an empty
' list by parking the bar's pair on the spacer row.
' ---------------------------------------------------------------------------

    DIM gm_fok            ' 1 if the last fetch produced a usable listing
    DIM gm_land           ' 0 = land on the first entry after a fetch, 1 = last
    DIM gm_i, gm_r
    DIM #gf_n
    DIM #gm_top_lo, #gm_top_hi   ' record 0's msgNum, before fmt_u32 eats it
    DIM #gp_base, gp_i
    DIM #gc_base, gc_o
    DIM gb_o
    DIM gi_row
    DIM #gi_w, #gi_flags

' ===========================================================================
' Fetch
' ===========================================================================

' gm_fetch_index: pull one IDX_MAX-entry window of the inbox into SC_IDX.
'
' aux1 = DIRECTORY and aux2 = 255 selects the raw 220-byte MailIndexItem
' listing. The human-readable format is the only one carrying the IMPORTANT
' flag, but it also right-pads every field into fixed columns whose widths
' shift between requests (numW is computed per batch from the largest
' msgNum returned), and it drops the timestamp entirely -- and the
' timestamp is what the whole read/unread model runs on. Raw it is.
gm_fetch_index: PROCEDURE
    gm_busy = 1
    GOSUB gm_fetching
    gm_fok = 0
    gm_count = 0
    #gm_ecode = 0

    ' "N:GMAIL:///INBOX?range=<start>-<end>". Absolute, 0-based, inclusive.
    #fn_txlen = 0
    #fn_src = VARPTR lit_idx_base(0) : ls_max = 32 : GOSUB fn_strlen : GOSUB fn_putstr
    #fm_hi = 0 : #fm_lo = #gm_range
    GOSUB fmt_u32
    #fn_src = #fm_p : ls_max = 12 : GOSUB fn_strlen : GOSUB fn_putstr
    #fn_src = VARPTR lit_dash(0) : ls_max = 4 : GOSUB fn_strlen : GOSUB fn_putstr
    #fm_hi = 0 : #fm_lo = #gm_range + IDX_MAX - 1
    GOSUB fmt_u32
    #fn_src = #fm_p : ls_max = 12 : GOSUB fn_strlen : GOSUB fn_putstr

    net_mode = MB_MODE_DIR
    net_trans = MB_FMT_RAW
    ' The open is where all the time goes: the adapter runs the label
    ' lookups, the messages.list paging and one messages.get per entry
    ' before it answers. TMO_NORM's 15 seconds is not enough.
    #fn_tmo = TMO_LONG
    GOSUB net_open
    #fn_tmo = TMO_NORM
    IF fn_ok = 0 THEN
        GOSUB gm_map_err
        RETURN
    END IF

    ' One STATUS, no settle poll -- the whole listing is already staged in
    ' the receive buffer by the time open returns (see net_status's header).
    GOSUB net_status
    IF fn_ok = 0 THEN
        ' An empty folder drains to END_OF_FILE with nothing available.
        ' That is a valid, successful, zero-message listing.
        IF #net_err <> ERR_EOF THEN
            #gm_ecode = #net_err
            GOSUB net_close
            RETURN
        END IF
    END IF

    ' Read exactly one record per net_read. The records are fixed-width and
    ' the mailbox RX window is 512 bytes, so reading REC_STRIDE at a time
    ' means a record can never straddle two reads and there is no
    ' reassembly buffer to carry.
    #gf_n = #net_avail / REC_STRIDE
    IF #gf_n > IDX_MAX THEN #gf_n = IDX_MAX

    gm_i = 0
gfi_loop:
    IF gm_i >= #gf_n THEN GOTO gfi_done
    #net_readlen = REC_STRIDE
    GOSUB net_read
    IF fn_ok = 0 THEN GOTO gfi_done
    IF #net_gotlen < REC_STRIDE THEN GOTO gfi_done
    GOSUB gm_parse_rec
    gm_i = gm_i + 1
    gm_count = gm_i
    GOTO gfi_loop

gfi_done:
    GOSUB net_close

    ' The listing is newest-first and msgNum is a 1-based position from the
    ' OLDEST message, so record 0 carries (total - rangeStart) and that is
    ' the only place the folder's size is available.
    IF gm_count > 0 THEN
        #gm_total_lo = #gm_top_lo + #gm_range
        #gm_total_hi = #gm_top_hi
        IF #gm_total_lo < #gm_top_lo THEN #gm_total_hi = #gm_total_hi + 1
    ELSE
        #gm_total_lo = 0
        #gm_total_hi = 0
    END IF

    gm_fok = 1
END

' gm_parse_rec: one 220-byte MailIndexItem, sitting in FN_RX, into entry
' gm_i of SC_IDX.
gm_parse_rec: PROCEDURE
    #gp_base = SC_IDX + gm_i * IDX_STRIDE

    ' msgNum, little-endian u32. Kept as ASCII from here on: it goes back
    ' out in a URL and is never arithmetic again, so rendering it once at
    ' parse time costs less than re-deriving it per keypress.
    #fm_lo = (PEEK(FN_RX + REC_MSGNUM) AND 255) + (PEEK(FN_RX + REC_MSGNUM + 1) AND 255) * 256
    #fm_hi = (PEEK(FN_RX + REC_MSGNUM + 2) AND 255) + (PEEK(FN_RX + REC_MSGNUM + 3) AND 255) * 256
    IF gm_i = 0 THEN
        #gm_top_lo = #fm_lo
        #gm_top_hi = #fm_hi
    END IF
    GOSUB fmt_u32
    #cs_src = #fm_p : #cs_dst = #gp_base + IDX_NUM : cs_max = 11
    GOSUB gm_copy_san

    ' Sender: displayName, falling back to the bare address when the From:
    ' header had no display-name part (the adapter leaves displayName empty
    ' in that case rather than duplicating the address into it).
    IF (PEEK(FN_RX + REC_NAME) AND 255) = 0 THEN
        #cs_src = FN_RX + REC_EMAIL
    ELSE
        #cs_src = FN_RX + REC_NAME
    END IF
    #cs_dst = #gp_base + IDX_NAME : cs_max = 31
    GOSUB gm_copy_san

    ' Subject: the wire field is 128 bytes but 63 is already three times
    ' what the bounce-scroll can show.
    #cs_src = FN_RX + REC_SUBJ : #cs_dst = #gp_base + IDX_SUBJ : cs_max = 63
    GOSUB gm_copy_san

    ' Timestamp: kept as raw wire bytes. Nothing here ever needs it as a
    ' number -- it is only ever compared against the high-water mark, and
    ' an 8-byte little-endian compare works directly on the bytes.
    FOR gp_i = 0 TO 7
        POKE (#gp_base + IDX_TS + gp_i), PEEK(FN_RX + REC_TS + gp_i) AND 255
    NEXT gp_i

    POKE (#gp_base + IDX_FLAGS), 0
END

' ===========================================================================
' Text composition
' ===========================================================================

' gm_compose: build "Sender: Subject" for entry gm_e into SC_ENTRY and set
' sc_len. Serves both the static row draw (truncated to 19 columns by
' scr_puts) and the bounce-scroll (which pans the full length).
gm_compose: PROCEDURE
    #gc_base = SC_IDX + gm_e * IDX_STRIDE
    #cs_src = #gc_base + IDX_NAME : #cs_dst = SC_ENTRY : cs_max = 31
    GOSUB gm_copy_san
    gc_o = cs_o
    POKE (SC_ENTRY + gc_o), 58 : gc_o = gc_o + 1   ' ':'
    POKE (SC_ENTRY + gc_o), 32 : gc_o = gc_o + 1   ' ' '
    #cs_src = #gc_base + IDX_SUBJ : #cs_dst = SC_ENTRY + gc_o : cs_max = 63
    GOSUB gm_copy_san
    sc_len = gc_o + cs_o
END

' gm_load_entry: scroll.bas's hook. No mailbox round trip -- the listing is
' cached in SC_IDX, so the full text is just recomposed.
gm_load_entry: PROCEDURE
    gm_e = gm_vpage * PAGE_VIS + gm_slot
    GOSUB gm_compose
END

' gb_append: append the NUL-terminated string at #fm_p to SC_PAGEIND.
gb_append: PROCEDURE
    #cs_src = #fm_p : #cs_dst = SC_PAGEIND + gb_o : cs_max = 11
    GOSUB gm_copy_san
    gb_o = gb_o + cs_o
END

' gm_build_pageind: "<first>-<last>/<total>", e.g. "17-32/1284".
gm_build_pageind: PROCEDURE
    gb_o = 0
    IF gm_count = 0 THEN
        POKE SC_PAGEIND, 0
        RETURN
    END IF
    #fm_hi = 0 : #fm_lo = #gm_range + 1
    GOSUB fmt_u32 : GOSUB gb_append
    POKE (SC_PAGEIND + gb_o), 45 : gb_o = gb_o + 1   ' '-'
    #fm_hi = 0 : #fm_lo = #gm_range + gm_count
    GOSUB fmt_u32 : GOSUB gb_append
    POKE (SC_PAGEIND + gb_o), 47 : gb_o = gb_o + 1   ' '/'
    #fm_hi = #gm_total_hi : #fm_lo = #gm_total_lo
    GOSUB fmt_u32 : GOSUB gb_append
    POKE (SC_PAGEIND + gb_o), 0
END

' ===========================================================================
' Window arithmetic
' ===========================================================================

' gm_calc_vis: how many entries are drawn on the current visible page.
gm_calc_vis: PROCEDURE
    IF gm_count <= gm_vpage * PAGE_VIS THEN
        gm_vis = 0
        RETURN
    END IF
    gm_vis = gm_count - gm_vpage * PAGE_VIS
    IF gm_vis > PAGE_VIS THEN gm_vis = PAGE_VIS
END

' gm_calc_next: is there a further range past this window? A short page is
' always the end of the list; otherwise compare against the folder total.
gm_calc_next: PROCEDURE
    gm_next = 0
    IF gm_count < IDX_MAX THEN RETURN
    IF #gm_total_hi > 0 THEN
        gm_next = 1
        RETURN
    END IF
    IF (#gm_range + IDX_MAX) < #gm_total_lo THEN gm_next = 1
END

' ===========================================================================
' Drawing
' ===========================================================================

gm_video_inbox: PROCEDURE
    MODE 0, CS_WHITE, CS_WHITE, CS_BLUE, CS_WHITE
    BORDER CS_WHITE
    WAIT
END

' gm_draw_icon: the read/unread envelope in column 0 of row gi_row.
'
' Column 0 of a list row can carry a color-stack advance -- the fixed one
' at LIST_START_ROW, or the bar's closing bit when the row above is
' selected -- so this preserves bit 13 instead of writing a whole word.
gm_draw_icon: PROCEDURE
    IF #gi_flags AND 1 THEN
        #gi_w = GRAM_BASE + GRAM_ENV_NEW * 8 + COL_ICON_NEW
    ELSE
        #gi_w = GRAM_BASE + GRAM_ENV_OLD * 8 + COL_ICON_OLD
    END IF
    #BACKTAB(gi_row * SCREEN_COLS) = #gi_w + (#BACKTAB(gi_row * SCREEN_COLS) AND CS_ADVANCE)
END

' gm_apply_stack: stamp all four advance bits in one pass. Only safe
' because gm_render_list's CLS zeroed every word first and this runs after
' the last draw -- scr_puts and PRINT write whole words and would wipe them.
gm_apply_stack: PROCEDURE
    #BACKTAB(screenpos(0, LIST_START_ROW)) = #BACKTAB(screenpos(0, LIST_START_ROW)) OR CS_ADVANCE
    IF gm_vis > 0 THEN
        GOSUB gm_bar_set
    ELSE
        ' Nothing to highlight, but the COUNT of advances matters as much
        ' as their positions: drop the bar's pair and the legend row lands
        ' on p2 (blue) instead of wrapping to p0. Park them on the last two
        ' cells of the blank spacer row.
        #BACKTAB(screenpos(18, LIST_SPACER_ROW)) = #BACKTAB(screenpos(18, LIST_SPACER_ROW)) OR CS_ADVANCE
        #BACKTAB(screenpos(19, LIST_SPACER_ROW)) = #BACKTAB(screenpos(19, LIST_SPACER_ROW)) OR CS_ADVANCE
    END IF
    #BACKTAB(screenpos(0, LEGEND_ROW)) = #BACKTAB(screenpos(0, LEGEND_ROW)) OR CS_ADVANCE
END

' gm_bar_set / gm_bar_clr: add or remove the selection bar's two advance
' bits for the CURRENT gm_slot -- so gm_bar_clr has to run before gm_slot
' moves. The text underneath is untouched; only bit 13 moves, so the cursor
' walks without anything being redrawn.
'
' LIST_START_ROW + gm_slot + 1 is always <= LIST_SPACER_ROW, which is the
' entire reason the spacer row exists.
gm_bar_set: PROCEDURE
    #BACKTAB(screenpos(ENTRY_COL, LIST_START_ROW + gm_slot)) = #BACKTAB(screenpos(ENTRY_COL, LIST_START_ROW + gm_slot)) OR CS_ADVANCE
    #BACKTAB(screenpos(0, LIST_START_ROW + gm_slot + 1)) = #BACKTAB(screenpos(0, LIST_START_ROW + gm_slot + 1)) OR CS_ADVANCE
END

gm_bar_clr: PROCEDURE
    #BACKTAB(screenpos(ENTRY_COL, LIST_START_ROW + gm_slot)) = #BACKTAB(screenpos(ENTRY_COL, LIST_START_ROW + gm_slot)) AND $DFFF
    #BACKTAB(screenpos(0, LIST_START_ROW + gm_slot + 1)) = #BACKTAB(screenpos(0, LIST_START_ROW + gm_slot + 1)) AND $DFFF
END

gm_render_list: PROCEDURE
    CLS
    GOSUB gm_calc_vis
    GOSUB gm_calc_next

    ' Columns 0-1 of rows 0-1 are left blank: the MOB logo sits there.
    PRINT AT screenpos(3, TITLE_ROW) COLOR COL_TITLE, "Gmail"
    PRINT AT screenpos(9, TITLE_ROW) COLOR COL_ACCENT, "Inbox"

    GOSUB gm_build_pageind
    #s_src = SC_PAGEIND : s_row = PAGE_ROW : s_col = 3 : s_max = 17
    s_col_color = COL_TITLE
    GOSUB scr_puts

    IF gm_vis = 0 THEN
        PRINT AT screenpos(4, 5) COLOR COL_TEXT, "No messages"
    ELSE
        FOR gm_r = 0 TO PAGE_VIS - 1
            IF gm_r < gm_vis THEN
                gm_e = gm_vpage * PAGE_VIS + gm_r
                GOSUB gm_compose
                s_row = LIST_START_ROW + gm_r
                #s_src = SC_ENTRY : s_col = ENTRY_COL : s_max = ENTRY_WIDTH
                IF gm_r = gm_slot THEN
                    s_col_color = COL_SELECT
                ELSE
                    s_col_color = COL_TEXT
                END IF
                GOSUB scr_puts
                #gi_flags = PEEK(SC_IDX + gm_e * IDX_STRIDE + IDX_FLAGS) AND 255
                gi_row = LIST_START_ROW + gm_r
                GOSUB gm_draw_icon
            END IF
        NEXT gm_r
    END IF

    PRINT AT screenpos(0, LEGEND_ROW) COLOR COL_LEGEND, "ENT:READ <>:PG 9:RFR"

    ' Advance bits LAST -- every draw above writes whole words.
    GOSUB gm_apply_stack

    ' Prime the bounce-scroll for whichever row the bar landed on.
    sc_row = LIST_START_ROW + gm_slot
    sc_col = ENTRY_COL : sc_max = ENTRY_WIDTH : sc_color = COL_SELECT
    sc_active = 0 : sc_idle = 0
END

' ===========================================================================
' Cursor movement
'
' Order is load-bearing. The STIC halts the CPU for the whole active
' display and hands it back at vertical blank, so a procedure that outruns
' one vblank gets frozen mid-update and the partial screen is what gets
' scanned out -- for a FULL frame, not a sliver. Between gm_bar_clr and
' gm_bar_set the screen carries two advances instead of four, which drops
' the legend row onto the wrong register and shifts every row below the
' bar, and there is no way to move the pair atomically. So the pair goes
' first, back to back, where a vblank is least likely to expire between
' them; the ~38 cells of scr_recolor that follow are safe to be interrupted
' because scr_recolor masks with AND $FFF8 and never touches bit 13.
'
' scroll_reset still has to precede gm_bar_clr: it repaints the row being
' left and scroll_draw re-arms that row's advance bit as it goes, so
' running it afterwards would leave a fifth advance behind.
' ===========================================================================
gm_move_up: PROCEDURE
    sc_color = COL_TEXT
    GOSUB scroll_reset

    GOSUB gm_bar_clr
    gm_slot = gm_slot - 1
    GOSUB gm_bar_set

    s_row = LIST_START_ROW + gm_slot + 1
    s_col = ENTRY_COL : s_max = ENTRY_WIDTH : s_col_color = COL_TEXT
    GOSUB scr_recolor
    s_row = LIST_START_ROW + gm_slot
    s_col_color = COL_SELECT
    GOSUB scr_recolor

    sc_row = LIST_START_ROW + gm_slot
    sc_col = ENTRY_COL : sc_max = ENTRY_WIDTH : sc_color = COL_SELECT
END

gm_move_down: PROCEDURE
    sc_color = COL_TEXT
    GOSUB scroll_reset

    GOSUB gm_bar_clr
    gm_slot = gm_slot + 1
    GOSUB gm_bar_set

    s_row = LIST_START_ROW + gm_slot - 1
    s_col = ENTRY_COL : s_max = ENTRY_WIDTH : s_col_color = COL_TEXT
    GOSUB scr_recolor
    s_row = LIST_START_ROW + gm_slot
    s_col_color = COL_SELECT
    GOSUB scr_recolor

    sc_row = LIST_START_ROW + gm_slot
    sc_col = ENTRY_COL : sc_max = ENTRY_WIDTH : sc_color = COL_SELECT
END

' ===========================================================================
' Screen state machine.
'
' Bare labels rather than an ON..GOSUB dispatcher, matching the news
' client: each screen re-issues its own MODE on entry and every path out of
' the loop is a GOTO, so nothing ever falls through into the next INCLUDE.
' ===========================================================================
inbox_enter:
    GOSUB gm_fetch_index
    IF gm_fok = 0 THEN
        GOSUB gm_error
        ' A failed refresh must not throw away a listing that is still on
        ' screen-worthy in SC_IDX; only a cold failure retries.
        IF gm_list_valid = 1 THEN GOTO inbox_show
        GOTO inbox_enter
    END IF

    GOSUB hwm_flags
    gm_list_valid = 1

    IF gm_land = 1 THEN
        IF gm_count > PAGE_VIS THEN
            gm_vpage = 1
        ELSE
            gm_vpage = 0
        END IF
        GOSUB gm_calc_vis
        IF gm_vis = 0 THEN
            gm_slot = 0
        ELSE
            gm_slot = gm_vis - 1
        END IF
    ELSE
        gm_vpage = 0
        gm_slot = 0
    END IF
    gm_land = 0

inbox_show:
    GOSUB gm_calc_vis
    IF gm_slot >= gm_vis THEN
        IF gm_vis = 0 THEN
            gm_slot = 0
        ELSE
            gm_slot = gm_vis - 1
        END IF
    END IF
    ' The mark may have moved while a message was open, which turns entries
    ' read in bulk -- recompute before the icons are drawn.
    GOSUB hwm_flags
    GOSUB gm_video_inbox
    GOSUB gm_render_list
    GOSUB gm_logo_show
    GOSUB in_flush

inbox_loop:
    WAIT
    IF gm_vis > 0 THEN GOSUB scroll_step
    GOSUB in_poll

    IF in_disc = DISC_UP THEN GOTO ib_up
    IF in_disc = DISC_DOWN THEN GOTO ib_down
    IF in_disc = DISC_LEFT THEN GOTO ib_pgup
    IF in_disc = DISC_RIGHT THEN GOTO ib_pgdn
    IF in_btn <> 0 THEN GOTO ib_open
    IF in_key = KEYPAD_ENTER THEN GOTO ib_open
    IF in_key = KEYPAD_9 THEN GOTO ib_refresh
    IF in_key = KEYPAD_CLEAR THEN GOTO ib_refresh
    GOTO inbox_loop

ib_up:
    IF gm_vis = 0 THEN GOTO inbox_loop
    IF gm_slot > 0 THEN
        GOSUB gm_move_up
        GOTO inbox_loop
    END IF
    IF gm_vpage > 0 THEN
        gm_vpage = 0
        gm_slot = PAGE_VIS - 1
        GOTO inbox_show
    END IF
    GOTO ib_prev_range

ib_down:
    IF gm_vis = 0 THEN GOTO inbox_loop
    IF gm_slot < gm_vis - 1 THEN
        GOSUB gm_move_down
        GOTO inbox_loop
    END IF
    IF gm_vpage = 0 THEN
        IF gm_count > PAGE_VIS THEN
            gm_vpage = 1
            gm_slot = 0
            GOTO inbox_show
        END IF
    END IF
    GOTO ib_next_range

ib_pgup:
    IF gm_vpage = 1 THEN
        gm_vpage = 0
        gm_slot = 0
        GOTO inbox_show
    END IF
    GOTO ib_prev_range

ib_pgdn:
    IF gm_vpage = 0 THEN
        IF gm_count > PAGE_VIS THEN
            gm_vpage = 1
            gm_slot = 0
            GOTO inbox_show
        END IF
    END IF
    GOTO ib_next_range

ib_prev_range:
    IF #gm_range = 0 THEN GOTO inbox_loop
    IF #gm_range < IDX_MAX THEN
        #gm_range = 0
    ELSE
        #gm_range = #gm_range - IDX_MAX
    END IF
    gm_land = 1
    GOTO inbox_enter

ib_next_range:
    IF gm_next = 0 THEN GOTO inbox_loop
    #gm_range = #gm_range + IDX_MAX
    gm_land = 0
    GOTO inbox_enter

ib_open:
    IF gm_vis = 0 THEN GOTO inbox_loop
    gm_sel = gm_vpage * PAGE_VIS + gm_slot
    GOTO msg_enter

ib_refresh:
    #gm_range = 0
    gm_land = 0
    GOTO inbox_enter
