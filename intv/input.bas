' input.bas -- edge-detected controller input, shared by both screens.
'
' Verbatim from fujinet-config/intv/input.bas's in_poll (NOT netcat's
' kbd.bas version -- see the diagonal-jitter note below, which is the one
' substantive difference between them and the reason this copy is the one
' to carry forward).
'
' Uses the unqualified CONT.* pseudo-variables, which OR together both
' controller ports -- this is a single-player utility, so there's no reason
' to force player 1's jack specifically.
'
' in_poll() populates, once per call:
'   in_disc  - DISC_UP/DOWN/LEFT/RIGHT on a fresh press or an auto-repeat
'              tick while held; 0 otherwise.
'   in_btn   - 1 on a fresh action-button press (any of B0/B1/B2); 0 otherwise.
'   in_key   - the decoded keypad digit/CLEAR/ENTER on a fresh press, or
'              KEYPAD_NONE (12) if nothing new.
'
' CONT.KEY's decode runs in the ISR, so in_poll must be called exactly once
' per frame, after WAIT.

    CONST IN_REPEAT_DELAY = 18   ' frames held before auto-repeat kicks in (~0.3s)
    CONST IN_REPEAT_RATE  = 6    ' frames between repeats once repeating (~0.1s)

    DIM in_disc, in_pdisc, in_rdelay
    DIM in_braw, in_btn,  in_pbtn
    DIM in_key,  in_pkey

' ---------------------------------------------------------------------------
' in_poll: call once per frame (after WAIT). Sets in_disc/in_btn/in_key.
' ---------------------------------------------------------------------------
in_poll: PROCEDURE
    ' --- disc, with auto-repeat while held in one direction ---
    in_disc = 0
    IF CONT.UP THEN in_disc = DISC_UP
    IF CONT.DOWN THEN in_disc = DISC_DOWN
    IF CONT.LEFT THEN in_disc = DISC_LEFT
    IF CONT.RIGHT THEN in_disc = DISC_RIGHT

    IF in_disc <> 0 THEN
        IF in_disc <> in_pdisc AND in_pdisc = 0 THEN
            in_rdelay = IN_REPEAT_DELAY     ' fresh press from neutral -- fire now
        ELSE
            ' Same direction held, OR slid/wobbled to a neighbour without
            ' releasing. Both go through the repeat gate, and that "OR" is the
            ' point: the disc has 16 positions and the diagonals set TWO
            ' cardinal bits, so ENE and NE read as both UP and RIGHT (jzintv
            ' src/pads/pads.c, "Pad bit 1 is set for SSE through NE / Pad bit 2
            ' is set for ENE through NW"). The tests above are last-match-wins,
            ' so a thumb parked near that boundary alternates in_disc between
            ' DISC_UP and DISC_RIGHT frame to frame. Treating a changed
            ' direction as a fresh press fires the handler every other frame
            ' instead of every seventh, which is what turns a list screen's
            ' redraw into a continuous flicker.
            '
            ' The cost is that changing direction without releasing the disc
            ' waits up to IN_REPEAT_RATE frames (~0.1s) instead of acting at
            ' once. Not perceptible, and it stops the cursor racing.
            IF in_rdelay > 0 THEN
                in_rdelay = in_rdelay - 1
                in_disc = 0
            ELSE
                in_rdelay = IN_REPEAT_RATE
            END IF
        END IF
    END IF
    in_pdisc = 0
    IF CONT.UP THEN in_pdisc = DISC_UP
    IF CONT.DOWN THEN in_pdisc = DISC_DOWN
    IF CONT.LEFT THEN in_pdisc = DISC_LEFT
    IF CONT.RIGHT THEN in_pdisc = DISC_RIGHT

    ' --- action buttons, edge-triggered (no repeat) ---
    in_braw = 0
    IF CONT.B0 OR CONT.B1 OR CONT.B2 THEN in_braw = 1
    IF in_braw <> 0 AND in_pbtn = 0 THEN
        in_btn = 1
    ELSE
        in_btn = 0
    END IF
    in_pbtn = in_braw

    ' --- keypad, edge-triggered ---
    in_key = KEYPAD_NONE
    IF CONT.KEY <> KEYPAD_NONE AND CONT.KEY <> in_pkey THEN
        in_key = CONT.KEY
    END IF
    in_pkey = CONT.KEY
END

' ---------------------------------------------------------------------------
' in_flush: drop any input currently held, so the next in_poll only reports
' genuinely new presses. Called after a blocking operation (a network fetch
' that ran for many seconds) so a button still held from before doesn't
' immediately re-trigger on the screen that follows.
' ---------------------------------------------------------------------------
in_flush: PROCEDURE
    WAIT
    in_pdisc = 0
    IF CONT.UP THEN in_pdisc = DISC_UP
    IF CONT.DOWN THEN in_pdisc = DISC_DOWN
    IF CONT.LEFT THEN in_pdisc = DISC_LEFT
    IF CONT.RIGHT THEN in_pdisc = DISC_RIGHT
    in_rdelay = IN_REPEAT_DELAY
    in_braw = 0
    IF CONT.B0 OR CONT.B1 OR CONT.B2 THEN in_braw = 1
    in_pbtn = in_braw
    in_pkey = CONT.KEY
    in_disc = 0 : in_btn = 0 : in_key = KEYPAD_NONE
END

' ---------------------------------------------------------------------------
' in_anykey: block until any control is pressed (and was not already held).
' Used by the error screen.
' ---------------------------------------------------------------------------
in_anykey: PROCEDURE
    GOSUB in_flush
ia_loop:
    WAIT
    GOSUB in_poll
    IF in_btn <> 0 THEN RETURN
    IF in_key <> KEYPAD_NONE THEN RETURN
    IF in_disc <> 0 THEN RETURN
    GOTO ia_loop
END
