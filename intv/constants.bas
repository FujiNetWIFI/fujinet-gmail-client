' constants.bas -- screen/color/input constants, GRAM cards, wire-record
' layout and the scratch-RAM map.
'
' Screen/color/disc/keypad constants are the standard IntyBASIC set already
' used throughout the FujiNet Intellivision tree (verbatim from
' fujinet-lobby/intv/constants.bas, itself from fujinet-config).
'
' *** RAM BUDGET RULE ***
' IntyBASIC's 228 8-bit / 47 16-bit scratchpad variables are off-limits for
' anything list-shaped. Every table (the message index, the wrapped body,
' the scroll buffer) lives in SC_* cart RAM and is PEEKed in place.
'
' *** 16-BIT PEEK RULE ***
' IntyBASIC v1.4.2 silently drops the AND 255 mask on an assignment of the
' shape "plain_var = PEEK(...) AND 255" when the destination is an 8-bit
' variable -- no ANDI is emitted at all and the peeked word's upper byte
' survives into the variable. EVERY PEEK in this program lands in a
' #-prefixed (16-bit) destination. See fujinet.bas's header for the full
' write-up.

    CONST BACKTAB_ADDR = $0200
    CONST SCREEN_ROWS  = 12
    CONST SCREEN_COLS  = 20

    DEF FN screenpos(aColumn, aRow)  = (((aRow)*SCREEN_COLS)+(aColumn))
    DEF FN screenaddr(aColumn, aRow) = (BACKTAB_ADDR+(((aRow)*SCREEN_COLS)+(aColumn)))

    ' -------------------------------------------------------------------
    ' Color-stack colors. Used both as a card's FOREGROUND (bits 0-2) and
    ' as a stack REGISTER (the four MODE arguments = backgrounds).
    ' -------------------------------------------------------------------
    CONST CS_BLACK      = $0000
    CONST CS_BLUE       = $0001
    CONST CS_RED        = $0002
    CONST CS_TAN        = $0003
    CONST CS_DARKGREEN  = $0004
    CONST CS_GREEN      = $0005
    CONST CS_YELLOW     = $0006
    CONST CS_WHITE      = $0007
    ' 8-15 are BACKGROUND ONLY: a GROM card's foreground is bits 0-2 plus
    ' bit 12, and bit 12 doubles as the Colored Squares selector, so the
    ' STIC requires foreground bit 3 to be 0 for GROM cards. Never use
    ' these as a text color.
    CONST CS_ADVANCE    = $2000   ' bit 13: advance the color stack one position

    ' -------------------------------------------------------------------
    ' Palette roles.
    '
    ' INBOX runs on a white ground (Gmail's own), so its text is BLACK and
    ' the selection bar inverts to WHITE on blue. MESSAGE runs blue header
    ' + tan "paper", so its body text is BLACK on tan and its header text
    ' is white/yellow on blue.
    ' -------------------------------------------------------------------
    CONST COL_TEXT      = CS_BLACK   ' list entries, on white
    CONST COL_SELECT    = CS_WHITE   ' selected entry, on the blue bar
    CONST COL_TITLE     = CS_BLACK   ' "Gmail" wordmark + page indicator
    CONST COL_ACCENT    = CS_RED     ' "Inbox"
    CONST COL_LEGEND    = CS_BLUE    ' key legend, on white
    CONST COL_ICON_NEW  = CS_RED     ' unread envelope
    CONST COL_ICON_OLD  = CS_TAN     ' read envelope (deliberately low contrast on white)
    CONST COL_MHDR      = CS_WHITE   ' message sender, on blue
    CONST COL_MSUBJ     = CS_YELLOW  ' message subject, on blue
    CONST COL_MPOS      = CS_YELLOW  ' scroll position, on blue
    CONST COL_MBODY     = CS_BLACK   ' message body, on tan
    CONST COL_MLEG      = CS_WHITE   ' message legend, on blue
    CONST COL_BOX       = CS_WHITE   ' status/error box text, on black
    CONST COL_BOXERR    = CS_RED

    ' -------------------------------------------------------------------
    ' GRAM cards. Four of the 64 are used.
    '
    ' GRAM_PILLAR/GRAM_DIAG are the LEFT HALF of the Gmail "M"; the right
    ' half is the same two cards drawn again with FLIPX (the M is
    ' left-right symmetric), which is what keeps the logo to two cards
    ' instead of four. See gfx.bas.
    ' -------------------------------------------------------------------
    CONST GRAM_PILLAR     = 0
    CONST GRAM_DIAG       = 1
    CONST GRAM_ENV_NEW    = 2   ' closed envelope = unread
    CONST GRAM_ENV_OLD    = 3   ' open envelope   = read
    CONST GRAM_BASE       = $0800  ' GRAM card N encodes as word $0800 + N*8 + color

    ' MOB (sprite) register bits. X register: $0100 interaction, $0200
    ' visible, $0400 double width. Y register: $0080 16-line sprite,
    ' bits 9-8 vertical scale (00=0.5x 01=1x 10=2x 11=4x), $0400 X flip,
    ' $0800 Y flip. SPR_ZOOMY2X is the 2x setting -- an 8x8 card drawn
    ' with ZOOMX2 + ZOOMY2X covers exactly 16x16 screen pixels = 2 cards
    ' wide by 2 cards tall.
    CONST SPR_VISIBLE   = $0200
    CONST SPR_ZOOMX2    = $0400
    CONST SPR_ZOOMY2X   = $0200
    CONST SPR_FLIPX     = $0400
    CONST SPR_BLUE      = $0001
    CONST SPR_RED       = $0002
    CONST SPR_GREEN     = $0005
    CONST SPR_YELLOW    = $0006

    ' Pixel offset from background card (0,0) to MOB coordinates. Measured
    ' against the emulator (fujinet-lobby/intv/constants.bas): both axes
    ' are offset by 8, i.e. MOB (8,8) is the top-left card. Don't infer
    ' this from the manual's coordinate ranges (X 0-168, Y 0-95) -- they
    ' imply an asymmetry that isn't there.
    CONST MOB_X0 = 8
    CONST MOB_Y0 = 8

    ' Disc directions.
    CONST DISC_UP     = $0004
    CONST DISC_RIGHT  = $0002
    CONST DISC_DOWN   = $0001
    CONST DISC_LEFT   = $0008

    ' Keypad, as DECODED by CONT.KEY.
    CONST KEYPAD_0      = 0
    CONST KEYPAD_9      = 9
    CONST KEYPAD_CLEAR  = 10
    CONST KEYPAD_ENTER  = 11
    CONST KEYPAD_NONE   = 12

    ' -------------------------------------------------------------------
    ' INBOX screen layout (20x12).
    '
    ' LIST_LAST_ROW is 9, not 10, because of the color stack: the blue
    ' selection bar needs a non-empty white run beneath it, so row 10 is a
    ' permanent blank spacer (see st_inbox.bas's gm_apply_stack).
    ' -------------------------------------------------------------------
    CONST TITLE_ROW       = 0
    CONST PAGE_ROW        = 1
    CONST LIST_START_ROW  = 2
    CONST LIST_LAST_ROW   = 9
    CONST LIST_SPACER_ROW = 10
    CONST LEGEND_ROW      = 11
    CONST PAGE_VIS        = 8    ' entries visible at once
    CONST ENTRY_COL       = 1    ' col 0 is the icon; the bar spans 1-19
    CONST ENTRY_WIDTH     = 19

    ' -------------------------------------------------------------------
    ' MESSAGE screen layout.
    ' -------------------------------------------------------------------
    CONST MSG_FROM_ROW  = 0
    CONST MSG_SUBJ_ROW  = 1    ' rows 1-2, wrapped to 2 rows of 21
    CONST MSG_SUBJ_ROWS = 2
    CONST MSG_BODY_ROW  = 3    ' rows 3-10
    CONST MSG_BODY_VIS  = 8
    CONST MSG_LEGEND_ROW = 11

    ' -------------------------------------------------------------------
    ' Index model.
    '
    ' 16 entries, not 20: exactly two 8-row visible pages (clean paging
    ' arithmetic), and every entry saved shortens the OPEN stall -- the
    ' adapter issues one messages.get round trip PER ENTRY inside OPEN
    ' (GMAIL.cpp folder_index), so a 16-entry listing is ~19 sequential
    ' HTTPS requests where a 20-entry one is ~23.
    ' -------------------------------------------------------------------
    CONST IDX_MAX    = 16
    CONST IDX_STRIDE = 120
    CONST IDX_NUM    = 0     '  12  msgNum as ASCII decimal, NUL-terminated
    CONST IDX_NAME   = 12    '  32  sender display name (31 + NUL)
    CONST IDX_SUBJ   = 44    '  64  subject, truncated (63 + NUL)
    CONST IDX_TS     = 108   '   8  timestamp, raw wire bytes, little-endian
    CONST IDX_FLAGS  = 116   '   1  bit 0 = unread (timestamp > high-water mark)

    ' -------------------------------------------------------------------
    ' Wire record: Mailbox.h's MailIndexItem, #pragma pack(1), 220 bytes.
    ' The static_assert(sizeof == 220) in that header is a deliberate
    ' tripwire against this layout changing -- if a firmware update trips
    ' it, this block and REC_STRIDE are what have to move.
    ' -------------------------------------------------------------------
    CONST REC_MSGNUM = 0     '   4  little-endian u32
    CONST REC_NAME   = 4     '  32  NUL-terminated, NUL-padded
    CONST REC_EMAIL  = 36    '  48  fallback when displayName is empty
    CONST REC_SUBJ   = 84    ' 128
    CONST REC_TS     = 212   '   8  little-endian u64, seconds since epoch
    CONST REC_STRIDE = 220

    ' Access modes (Protocol.h ACCESS_MODE) and aux2 semantics.
    ' On a DIRECTORY open aux2 is a FORMAT selector: 255 = raw structs,
    ' anything else = human-readable at that column width. On a READ open
    ' aux2 is the real netProtoTranslation_t.
    CONST MB_MODE_READ  = 4
    CONST MB_MODE_DIR   = 6
    CONST MB_FMT_RAW    = 255
    CONST MB_TRANS_NONE = 0
    CONST MB_TRANS_CRLF = 3

    ' NDEV_STATUS error codes (status_error_codes.h) this client maps.
    CONST ERR_SUCCESS   = 1
    CONST ERR_EOF       = 136   ' buffer drained -- normal end of stream
    CONST ERR_NOTFOUND  = 170
    CONST ERR_DENIED    = 167
    CONST ERR_NOAUTH    = 212
    CONST ERR_NOSERVICE = 210

    ' Transaction timeouts, in frames. The default 15s is fine for
    ' everything except a GMAIL OPEN, which does all of its HTTPS work
    ' synchronously before answering.
    CONST TMO_NORM = 900     ' 15s
    CONST TMO_LONG = 3600    ' 60s

    ' Message body model: wrapped on ingest to fixed 20-column rows, raw
    ' text discarded. BODY_STRIDE is 21 because wrap.bas emits 20
    ' characters + NUL per row.
    CONST BODY_ROWS   = 128
    CONST BODY_STRIDE = 21
    CONST LINE_CAP    = 160  ' longest raw line accumulated before a hard flush

    ' -------------------------------------------------------------------
    ' Scratch RAM ($8000-$9BFF) -- ours, outside the mailbox proper
    ' ($9C00+). $9100-$917F is left free as fujinet.bas's traditional
    ' region; nothing here reaches into it.
    ' -------------------------------------------------------------------
    CONST SC_IDX     = $8000  ' 1920  16 x 120 parsed index entries
    CONST SC_ENTRY   = $8780  '  128  "Name: Subject", bounce-scroll source
    CONST SC_NUM32   = $8800  '   12  fmt_u32 digit scratch
    CONST SC_HWM     = $8810  '    8  high-water timestamp, wire LE order
    CONST SC_LINE    = $8820  '  176  raw body line accumulator (LINE_CAP + slack)
    CONST SC_PAGEIND = $88D0  '   32  "1-16/1234"
    CONST SC_SUBJW   = $88F0  '   48  message subject wrapped, 2 x 21
    '                  $8920-$90FF free
    '                  $9100-$917F reserved (fujinet.bas convention)
    CONST SC_BODY    = $9180  ' 2688  128 x 21 wrapped body rows, ends at $9BFF
