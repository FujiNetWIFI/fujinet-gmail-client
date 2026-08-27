' screen.bas -- low-level text drawing helpers shared by every screen.
'
' Character encoding follows the standard IntyBASIC card formula used
' throughout the FujiNet Intellivision tree:
'     card = ascii - 32
'     screen word = card*8 + color
' In COLOR STACK mode that reaches GROM cards 0-94, i.e. ASCII 32-126 --
' which INCLUDES lowercase (cards 65-90 are 'a'-'z'). That is the whole
' reason this client runs in color stack mode: mail is mixed case, and
' Foreground/Background mode would cap it at card 63 (uppercase only) and
' force every message through a case fold.
'
' So the printable clamp here is 126, NOT the 95 that fujinet-news's copy
' uses -- news folds to uppercase during ingest and never needs lowercase
' cards. Anything outside 32-126 has already been substituted during
' ingest (gmail.bas's sanitize collapses UTF-8 runs to '?'); the clamp is
' a last-resort guard, not the primary filter.
'
' Nothing here ever buffers a whole string in an IntyBASIC variable -- text
' is always drawn straight from a source address (ROM DATA or scratch RAM)
' via PEEK, one character at a time.

    DIM s_row, s_col, s_i, s_len, s_max, s_col_color
    DIM #s_src, #s_val, #s_c

' ---------------------------------------------------------------------------
' scr_row_clear: blank row s_row (all 20 columns). Clears bit 13 with it,
' so any advance bit on that row has to be re-armed afterwards.
' ---------------------------------------------------------------------------
scr_row_clear: PROCEDURE
    FOR s_i = 0 TO SCREEN_COLS - 1
        #BACKTAB(s_row * SCREEN_COLS + s_i) = CS_BLACK
    NEXT s_i
END

' ---------------------------------------------------------------------------
' scr_puts: draw bytes from #s_src onto row s_row starting at column s_col,
' in color s_col_color, stopping at the first NUL or after s_max characters,
' then space-padding the remainder of the s_max-wide field. Sets s_len to
' the number of real (non-pad) characters drawn. Caller ensures
' s_col + s_max <= SCREEN_COLS.
'
' Writes whole BACKTAB words, so it CLEARS bit 13 on every cell it touches.
' Every caller that draws over a color-stack advance must re-arm it after.
' ---------------------------------------------------------------------------
scr_puts: PROCEDURE
    s_len = 0
    WHILE (s_len < s_max) AND ((PEEK(#s_src + s_len) AND 255) <> 0)
        s_len = s_len + 1
    WEND
    FOR s_i = 0 TO s_max - 1
        IF s_i < s_len THEN
            #s_c = PEEK(#s_src + s_i) AND 255
        ELSE
            #s_c = 32
        END IF
        IF #s_c < 32 OR #s_c > 126 THEN #s_c = 32
        #BACKTAB(s_row * SCREEN_COLS + s_col + s_i) = (#s_c - 32) * 8 + s_col_color
    NEXT s_i
END

' ---------------------------------------------------------------------------
' scr_recolor: change only the COLOR of s_max already-drawn characters on
' row s_row starting at column s_col, to s_col_color -- the card (glyph)
' underneath is left untouched, and so is bit 13 (the AND $FFF8 mask keeps
' both the card number and the advance bit).
'
' This is what makes moving the selection bar cheap: re-highlighting a row
' never redraws its text, so the cursor walks at ~19 cell writes per move
' instead of a full-list repaint. One BACKTAB read-modify-write costs about
' 264 cycles and an NTSC frame leaves the CPU roughly 13500, so the budget
' is ~50 cells per frame -- a 160-cell list repaint takes three frames and
' is visibly seen painting.
' ---------------------------------------------------------------------------
scr_recolor: PROCEDURE
    FOR s_i = 0 TO s_max - 1
        #s_val = (#BACKTAB(s_row * SCREEN_COLS + s_col + s_i) AND $FFF8) + s_col_color
        #BACKTAB(s_row * SCREEN_COLS + s_col + s_i) = #s_val
    NEXT s_i
END
