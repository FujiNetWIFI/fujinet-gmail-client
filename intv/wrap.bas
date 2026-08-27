' wrap.bas -- greedy whole-word wrap into 20-column rows.
'
' Verbatim from fujinet-news/clients/intv/wrap.bas.
'
' Nothing pre-wraps mail: the GMAIL adapter hands back a message body with
' its original line structure intact (Mailbox.cpp only translates line
' endings), and subjects arrive as one long field. Both get wrapped
' client-side here -- whole words moved to the next row, never split
' mid-word. st_msg.bas calls this once per raw body line during ingest, so
' the raw text is never stored, only the wrapped rows.
'
' Contract:
'   #w_src  IN/OUT: address of a NUL-terminated, already-sanitized field.
'           On return it points at that field's NUL (callers advance past
'           it with #w_src + 1).
'   #w_dst  IN: destination base. Output is w_rows rows of stride 21 --
'           up to 20 characters + NUL each, ready for scr_puts (s_max=20).
'   w_rows  IN: row budget (3 for headlines, 2 for the reader title).
' Text beyond the budget is dropped and the last row ellipsized ("...").
' Unused rows get a NUL at offset 0 so stale buffer content never shows.
' Words longer than 20 characters (rare: URLs) are hard-split at column 20.

    DIM w_rows, w_row, w_col, w_len, w_i
    DIM #w_src, #w_dst, #w_c

wrap_text: PROCEDURE
    w_row = 0
    w_col = 0
    FOR w_i = 0 TO w_rows - 1
        POKE #w_dst + w_i * 21, 0
    NEXT w_i

wt_word:
    ' Skip inter-word spaces, then bail at end of field.
    WHILE (PEEK(#w_src) AND 255) = 32
        #w_src = #w_src + 1
    WEND
    #w_c = PEEK(#w_src) AND 255
    IF #w_c = 0 THEN GOTO wt_done

    ' Measure the word (chars up to the next space or NUL).
    w_len = 0
wt_meas:
    #w_c = PEEK(#w_src + w_len) AND 255
    IF #w_c = 0 THEN GOTO wt_meas_done
    IF #w_c = 32 THEN GOTO wt_meas_done
    w_len = w_len + 1
    GOTO wt_meas
wt_meas_done:

    ' Whole word won't fit after a separator: close this row, start the
    ' next. (A word at w_col = 0 always starts in place -- the >20 case is
    ' hard-split by the copy loop below.)
    IF w_col > 0 THEN
        IF (w_col + 1 + w_len) > 20 THEN
            POKE #w_dst + w_row * 21 + w_col, 0
            w_row = w_row + 1
            w_col = 0
            IF w_row = w_rows THEN GOTO wt_overflow
        ELSE
            POKE #w_dst + w_row * 21 + w_col, 32
            w_col = w_col + 1
        END IF
    END IF

    ' Copy the word, hard-splitting at column 20 if it alone is too long.
    FOR w_i = 0 TO w_len - 1
        IF w_col = 20 THEN
            POKE #w_dst + w_row * 21 + 20, 0
            w_row = w_row + 1
            w_col = 0
            IF w_row = w_rows THEN GOTO wt_overflow
        END IF
        POKE #w_dst + w_row * 21 + w_col, PEEK(#w_src + w_i) AND 255
        w_col = w_col + 1
    NEXT w_i
    #w_src = #w_src + w_len
    GOTO wt_word

wt_overflow:
    ' Out of rows with text left over: ellipsize the last row, then advance
    ' #w_src to the field's NUL so the caller's cursor stays in sync. The
    ' dots go right after the row's text (its NUL can sit anywhere -- a row
    ' closed by a word that didn't fit ends early), clamped to column 17 so
    ' they always fit before the stride-21 boundary.
    w_col = 0
    WHILE (w_col < 17) AND ((PEEK(#w_dst + (w_rows - 1) * 21 + w_col) AND 255) <> 0)
        w_col = w_col + 1
    WEND
    POKE #w_dst + (w_rows - 1) * 21 + w_col, 46
    POKE #w_dst + (w_rows - 1) * 21 + w_col + 1, 46
    POKE #w_dst + (w_rows - 1) * 21 + w_col + 2, 46
    POKE #w_dst + (w_rows - 1) * 21 + w_col + 3, 0
    WHILE (PEEK(#w_src) AND 255) <> 0
        #w_src = #w_src + 1
    WEND
    RETURN

wt_done:
    IF w_row < w_rows THEN POKE #w_dst + w_row * 21 + w_col, 0
END
