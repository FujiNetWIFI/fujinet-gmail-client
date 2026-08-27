' st_msg.bas -- the MESSAGE screen: body fetch, wrap-on-ingest, scrolling.
'
' ---------------------------------------------------------------------------
' COLOR STACK STRUCTURE
'
' MODE 0, CS_BLUE, CS_TAN, CS_BLUE, CS_BLUE, with only two advance bits:
'
'     p0  rows 0-2                    header: sender, position, subject (blue)
'   advance at (0, MSG_BODY_ROW)
'     p1  rows 3-10                   the message "paper" (tan)
'   advance at (0, MSG_LEGEND_ROW)
'     p2  row 11                      legend (blue)
'
' p3 is unused. The advance at (0,3) sits in the body window's own top-left
' cell, so gm_msg_body_draw -- which repaints that cell on every scroll --
' has to re-arm it every time.
' ---------------------------------------------------------------------------

    DIM bd_llen           ' bytes accumulated in SC_LINE
    DIM bd_hb             ' 1 while inside a run of bytes above 126
    DIM bd_trunc          ' 1 if the body outran BODY_ROWS
    DIM bd_r, bd_w
    DIM #bd_rows          ' wrapped rows produced
    DIM #bd_top           ' first row shown in the 8-row window
    DIM #bd_i, #bd_c, #bd_n

' ===========================================================================
' Fetch and ingest
' ===========================================================================

' gm_fetch_body: read the selected message and wrap it into SC_BODY.
'
' aux2 = CRLF translation. The Intellivision reaches the adapter through
' the fujiversal RS232 build, whose native line ending is a bare LF, so the
' firmware rewrites mail's CRLF to a single LF and this ingest only ever
' has to recognise one terminator byte (a lone LF passes through that
' translation unchanged, and a stray lone CR is dropped here anyway).
'
' The raw text is never stored. Each line is wrapped into fixed 20-column
' rows as it arrives and then discarded -- SC_BODY's 128 rows are the only
' copy, which is what lets a 2.6KB body fit in a console with 2.7KB of
' usable cart RAM left after the index.
gm_fetch_body: PROCEDURE
    gm_busy = 2
    GOSUB gm_fetching
    gm_fok = 0
    #gm_ecode = 0
    #bd_rows = 0
    bd_llen = 0
    bd_hb = 0
    bd_trunc = 0

    ' "N:GMAIL:///INBOX/<msgnum>" -- the digits go out exactly as they came
    ' in, never re-derived from a number.
    #fn_txlen = 0
    #fn_src = VARPTR lit_body_base(0) : ls_max = 24 : GOSUB fn_strlen : GOSUB fn_putstr
    #fn_src = SC_IDX + gm_sel * IDX_STRIDE + IDX_NUM : ls_max = 12
    GOSUB fn_strlen : GOSUB fn_putstr

    net_mode = MB_MODE_READ
    net_trans = MB_TRANS_CRLF
    ' Fetching an OLD message is the slow case: message_id_for_seq has to
    ' page messages.list down to the requested position, 500 ids at a time.
    #fn_tmo = TMO_LONG
    GOSUB net_open
    #fn_tmo = TMO_NORM
    IF fn_ok = 0 THEN
        GOSUB gm_map_err
        RETURN
    END IF

    GOSUB net_status
    IF fn_ok = 0 THEN
        ' A message with no text part stages an empty buffer and still
        ' succeeds -- that reads back as END_OF_FILE, not an error.
        IF #net_err <> ERR_EOF THEN
            #gm_ecode = #net_err
            GOSUB net_close
            RETURN
        END IF
    END IF

gb_read:
    IF #net_avail = 0 THEN GOTO gb_done
    IF bd_trunc = 1 THEN GOTO gb_done
    #net_readlen = #net_avail
    IF #net_readlen > 512 THEN #net_readlen = 512
    GOSUB net_read
    IF fn_ok = 0 THEN GOTO gb_done
    IF #net_gotlen = 0 THEN GOTO gb_done

    #bd_i = 0
gb_byte:
    IF #bd_i >= #net_gotlen THEN GOTO gb_more
    #bd_c = PEEK(FN_RX + #bd_i) AND 255
    #bd_i = #bd_i + 1
    GOSUB gm_ingest
    IF bd_trunc = 1 THEN GOTO gb_done
    GOTO gb_byte

gb_more:
    ' fn_ok drops to 0 here once the buffer drains (#net_err becomes
    ' END_OF_FILE) -- that ends the loop as end of stream, not failure.
    GOSUB net_status
    GOTO gb_read

gb_done:
    ' A body that did not end with a newline still has a line pending.
    IF bd_llen > 0 THEN GOSUB gm_flush_line
    ' Abandoning a partly-drained buffer is safe: Mailbox's read() is a
    ' no-op over a buffer that close() throws away.
    GOSUB net_close
    gm_fok = 1
END

' gm_ingest: fold one raw byte (#bd_c) into the current line.
'
' Bytes above 126 are UTF-8 continuation garbage on this console -- the
' adapter does no charset conversion and no RFC 2047 decoding. A whole RUN
' of them collapses to a single '?', so one accented character costs one
' placeholder rather than two or three.
gm_ingest: PROCEDURE
    IF #bd_c = 13 THEN RETURN
    IF #bd_c = 10 THEN
        bd_hb = 0
        GOSUB gm_flush_line
        RETURN
    END IF
    IF #bd_c > 126 THEN
        IF bd_hb = 1 THEN RETURN
        bd_hb = 1
        #bd_c = 63                     ' '?'
    ELSE
        bd_hb = 0
        IF #bd_c = 9 THEN #bd_c = 32   ' TAB
        IF #bd_c < 32 THEN RETURN
    END IF
    POKE (SC_LINE + bd_llen), #bd_c
    bd_llen = bd_llen + 1
    IF bd_llen >= LINE_CAP THEN GOSUB gm_flush_line
END

' gm_flush_line: wrap SC_LINE into SC_BODY and reset the accumulator.
gm_flush_line: PROCEDURE
    IF #bd_rows >= BODY_ROWS THEN
        bd_trunc = 1
        bd_llen = 0
        RETURN
    END IF
    POKE (SC_LINE + bd_llen), 0

    IF bd_llen = 0 THEN
        ' Blank line: one blank row, so paragraph breaks survive the wrap.
        POKE (SC_BODY + #bd_rows * BODY_STRIDE), 0
        #bd_rows = #bd_rows + 1
        RETURN
    END IF

    ' Cap the row budget handed to wrap_text. It zeroes w_rows rows up
    ' front, so passing the whole remaining body (up to 128) would re-clear
    ' the entire buffer on every single line. A LINE_CAP-long line can only
    ' ever produce 9 rows of 20 columns, so 10 is a ceiling with slack.
    w_rows = 10
    #bd_n = BODY_ROWS - #bd_rows
    IF w_rows > #bd_n THEN w_rows = #bd_n
    #w_src = SC_LINE
    #w_dst = SC_BODY + #bd_rows * BODY_STRIDE
    GOSUB wrap_text

    ' wrap_text leaves w_row on the last row it wrote -- except on
    ' overflow, where it has already been stepped past the budget.
    bd_w = w_row + 1
    IF bd_w > w_rows THEN bd_w = w_rows
    #bd_rows = #bd_rows + bd_w
    bd_llen = 0
    IF #bd_rows >= BODY_ROWS THEN bd_trunc = 1
END

' ===========================================================================
' Drawing
' ===========================================================================

gm_video_msg: PROCEDURE
    MODE 0, CS_BLUE, CS_TAN, CS_BLUE, CS_BLUE
    BORDER CS_BLUE
    WAIT
END

' gm_draw_pos: "3/12" (page of pages) in the header's right-hand corner,
' with a trailing '+' when the body was truncated at BODY_ROWS.
gm_draw_pos: PROCEDURE
    gb_o = 0
    #fm_hi = 0 : #fm_lo = #bd_top / MSG_BODY_VIS + 1
    GOSUB fmt_u32 : GOSUB gb_append
    POKE (SC_PAGEIND + gb_o), 47 : gb_o = gb_o + 1   ' '/'
    #fm_hi = 0
    #fm_lo = (#bd_rows + MSG_BODY_VIS - 1) / MSG_BODY_VIS
    IF #fm_lo = 0 THEN #fm_lo = 1
    GOSUB fmt_u32 : GOSUB gb_append
    IF bd_trunc = 1 THEN
        POKE (SC_PAGEIND + gb_o), 43 : gb_o = gb_o + 1   ' '+'
    END IF
    POKE (SC_PAGEIND + gb_o), 0

    #s_src = SC_PAGEIND
    s_row = MSG_FROM_ROW : s_col = 13 : s_max = 7 : s_col_color = COL_MPOS
    GOSUB scr_puts
END

' gm_msg_body_draw: repaint the 8-row body window from SC_BODY. Called on
' entry and on every scroll -- a user-initiated repaint of 160 cells, which
' is well outside the per-frame budget but happens between frames, not
' during animation.
gm_msg_body_draw: PROCEDURE
    FOR bd_r = 0 TO MSG_BODY_VIS - 1
        s_row = MSG_BODY_ROW + bd_r
        s_col = 0 : s_max = SCREEN_COLS : s_col_color = COL_MBODY
        #bd_n = #bd_top + bd_r
        IF #bd_n < #bd_rows THEN
            #s_src = SC_BODY + #bd_n * BODY_STRIDE
            GOSUB scr_puts
        ELSE
            GOSUB scr_row_clear
        END IF
    NEXT bd_r

    IF #bd_rows = 0 THEN
        PRINT AT screenpos(0, MSG_BODY_ROW) COLOR COL_MBODY, "(no text content)"
    END IF

    ' The body window's top-left cell carries the advance into the tan
    ' band, and everything above just wrote whole words over it.
    #BACKTAB(screenpos(0, MSG_BODY_ROW)) = #BACKTAB(screenpos(0, MSG_BODY_ROW)) OR CS_ADVANCE
END

gm_msg_render: PROCEDURE
    CLS

    #s_src = SC_IDX + gm_sel * IDX_STRIDE + IDX_NAME
    s_row = MSG_FROM_ROW : s_col = 0 : s_max = 13 : s_col_color = COL_MHDR
    GOSUB scr_puts
    GOSUB gm_draw_pos

    #w_src = SC_IDX + gm_sel * IDX_STRIDE + IDX_SUBJ
    #w_dst = SC_SUBJW
    w_rows = MSG_SUBJ_ROWS
    GOSUB wrap_text
    FOR bd_r = 0 TO MSG_SUBJ_ROWS - 1
        #s_src = SC_SUBJW + bd_r * 21
        s_row = MSG_SUBJ_ROW + bd_r
        s_col = 0 : s_max = SCREEN_COLS : s_col_color = COL_MSUBJ
        GOSUB scr_puts
    NEXT bd_r

    GOSUB gm_msg_body_draw

    PRINT AT screenpos(0, MSG_LEGEND_ROW) COLOR COL_MLEG, "^v:LINE <>:PG CLR:BK"

    ' Advance bits last.
    #BACKTAB(screenpos(0, MSG_BODY_ROW)) = #BACKTAB(screenpos(0, MSG_BODY_ROW)) OR CS_ADVANCE
    #BACKTAB(screenpos(0, MSG_LEGEND_ROW)) = #BACKTAB(screenpos(0, MSG_LEGEND_ROW)) OR CS_ADVANCE
END

' ===========================================================================
' Screen state machine
' ===========================================================================
msg_enter:
    GOSUB gm_fetch_body
    IF gm_fok = 0 THEN
        GOSUB gm_error
        ' SC_IDX is untouched by a body fetch, so the listing is still
        ' good -- go back to it rather than refetching.
        GOTO inbox_show
    END IF
    ' Advance the high-water mark before the message is shown, so a reset
    ' mid-read still counts it as read.
    GOSUB hwm_update
    #bd_top = 0

    GOSUB gm_logo_hide
    GOSUB gm_video_msg
    GOSUB gm_msg_render
    GOSUB in_flush

msg_loop:
    WAIT
    GOSUB in_poll
    IF in_disc = DISC_UP THEN GOTO ms_up
    IF in_disc = DISC_DOWN THEN GOTO ms_down
    IF in_disc = DISC_LEFT THEN GOTO ms_pgup
    IF in_disc = DISC_RIGHT THEN GOTO ms_pgdn
    IF in_key = KEYPAD_CLEAR THEN GOTO inbox_show
    IF in_btn <> 0 THEN GOTO inbox_show
    GOTO msg_loop

ms_up:
    IF #bd_top = 0 THEN GOTO msg_loop
    #bd_top = #bd_top - 1
    GOTO ms_redraw

ms_down:
    IF (#bd_top + MSG_BODY_VIS) >= #bd_rows THEN GOTO msg_loop
    #bd_top = #bd_top + 1
    GOTO ms_redraw

ms_pgup:
    IF #bd_top = 0 THEN GOTO msg_loop
    IF #bd_top < MSG_BODY_VIS THEN
        #bd_top = 0
    ELSE
        #bd_top = #bd_top - MSG_BODY_VIS
    END IF
    GOTO ms_redraw

ms_pgdn:
    IF (#bd_top + MSG_BODY_VIS) >= #bd_rows THEN GOTO msg_loop
    #bd_top = #bd_top + MSG_BODY_VIS
    IF (#bd_top + MSG_BODY_VIS) > #bd_rows THEN #bd_top = #bd_rows - MSG_BODY_VIS
    GOTO ms_redraw

ms_redraw:
    GOSUB gm_msg_body_draw
    GOSUB gm_draw_pos
    GOTO msg_loop
