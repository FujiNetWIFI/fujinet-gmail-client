' gfx.bas -- the four GRAM cards and the MOB Gmail logo.
'
' Intellivision GRAM cards are 8x8, one byte per row, MSB leftmost. Four of
' the 64 available are used; the other 60 stay free.
'
' Safe to sit here as raw data: gmail.bas's GOTO gm_start jumps clear of
' every INCLUDE, so straight-line execution never falls into it.

' ---------------------------------------------------------------------------
' THE LOGO
'
' The Gmail "M" is left-right symmetric, so only its LEFT half is stored:
' one card for the vertical pillar, one for the descending diagonal. The
' right half is those same two cards drawn again with FLIPX. Two cards and
' four MOBs instead of four cards, and the halves can never drift apart.
'
' All four MOBs sit at the same place -- MOB (8,8), i.e. background card
' (0,0) -- with ZOOMX2 and the 2x vertical scale, so each 8x8 card covers
' 16x16 screen pixels: columns 0-1, rows 0-1. The unflipped pair draws the
' left half, the flipped pair mirrors into the right half of that same
' 16-pixel span.
'
' Colors are Gmail's own: blue and green pillars, red and yellow diagonals.
' The inbox screen's color stack sits on white behind them, which is what
' makes drawing only the colored strokes enough -- there is no envelope
' body to fill.
'
' MOB priority is by index (lower wins), and the diagonals are 0 and 1 so
' the red and yellow strokes cross OVER the pillars where they overlap at
' the top, matching the real mark's fold.
' ---------------------------------------------------------------------------
gm_gram:
    ' GRAM_PILLAR -- left vertical, 2px wide, full height. At ZOOMX2 that
    ' lands as a 4-pixel-wide bar on screen.
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"
    BITMAP "##......"

    ' GRAM_DIAG -- the left stroke of the M's inner V, descending from the
    ' top of the pillar to the bottom centre. Mirrored, the two strokes
    ' meet exactly on the 16-pixel centre line.
    BITMAP "##......"
    BITMAP "###....."
    BITMAP ".###...."
    BITMAP "..###..."
    BITMAP "...###.."
    BITMAP "....###."
    BITMAP ".....###"
    BITMAP "......##"

    ' GRAM_ENV_NEW -- closed envelope (unread): sealed rectangle with the
    ' flap crease converging to the centre.
    BITMAP "........"
    BITMAP "########"
    BITMAP "##....##"
    BITMAP "#.#..#.#"
    BITMAP "#..##..#"
    BITMAP "#......#"
    BITMAP "########"
    BITMAP "........"

    ' GRAM_ENV_OLD -- open envelope (read): flap lifted clear of the body.
    BITMAP "..####.."
    BITMAP ".#....#."
    BITMAP "#......#"
    BITMAP "########"
    BITMAP "#......#"
    BITMAP "#......#"
    BITMAP "########"
    BITMAP "........"

' ---------------------------------------------------------------------------
' gm_define_gram: upload all four cards. Called once at boot before
' anything draws -- until the DEFINE lands, GRAM holds whatever the EXEC
' left there. DEFINE takes effect on the NEXT frame, hence the WAIT. Four
' cards is comfortably inside the ~18-per-frame ceiling.
' ---------------------------------------------------------------------------
gm_define_gram: PROCEDURE
    WAIT
    DEFINE GRAM_PILLAR, 4, gm_gram
    WAIT
END

' ---------------------------------------------------------------------------
' gm_logo_show / gm_logo_hide: the logo is only meaningful on the inbox, so
' it is torn down on the way to any other screen. MOBs persist across a CLS
' -- they are STIC registers, not BACKTAB -- so leaving them up would park
' a Gmail M over the message header.
' ---------------------------------------------------------------------------
gm_logo_show: PROCEDURE
    SPRITE 0, MOB_X0 + SPR_VISIBLE + SPR_ZOOMX2, MOB_Y0 + SPR_ZOOMY2X, GRAM_BASE + GRAM_DIAG * 8 + SPR_RED
    SPRITE 1, MOB_X0 + SPR_VISIBLE + SPR_ZOOMX2, MOB_Y0 + SPR_ZOOMY2X + SPR_FLIPX, GRAM_BASE + GRAM_DIAG * 8 + SPR_YELLOW
    SPRITE 2, MOB_X0 + SPR_VISIBLE + SPR_ZOOMX2, MOB_Y0 + SPR_ZOOMY2X, GRAM_BASE + GRAM_PILLAR * 8 + SPR_BLUE
    SPRITE 3, MOB_X0 + SPR_VISIBLE + SPR_ZOOMX2, MOB_Y0 + SPR_ZOOMY2X + SPR_FLIPX, GRAM_BASE + GRAM_PILLAR * 8 + SPR_GREEN
END

gm_logo_hide: PROCEDURE
    SPRITE 0, 0, 0, 0
    SPRITE 1, 0, 0, 0
    SPRITE 2, 0, 0, 0
    SPRITE 3, 0, 0, 0
END
