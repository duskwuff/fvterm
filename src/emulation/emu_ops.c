#include "emu_utils.h"
#include "DefaultColors.h"

static void row_fill(struct termRow *row, int start, int count, uint64_t value)
{
#ifdef NOT_DARWIN
    // bah, we have to do this the hard way
    for(int i = 0; i < count; i++)
        row->chars[start + i] = value;
#else
    // memset_pattern8 is highly optimized on x86 :)
    memset_pattern8(&row->chars[start], &value, count * 8);
#endif
    row->flags = TERMROW_DIRTY;
}


static void scroll_down(struct emuState *S, int top, int btm, int count)
{
    assert(count > 0);
    int clearStart;
    if(count > btm - top) {
        // every row's getting cleared, so we don't need to bother
        // changing the pointers at all!
        clearStart = top;
    } else {
        clearStart = btm - count + 1;
        struct termRow *keepBuf[count];
        memcpy(keepBuf, &S->rows[top],
                count * sizeof(struct termRow *));
        memmove(&S->rows[top], &S->rows[top + count],
                (clearStart - top) * sizeof(struct termRow *));
        memcpy(&S->rows[clearStart], keepBuf,
                count * sizeof(struct termRow *));
    }

    for(int i = clearStart; i <= btm; i++)
        row_fill(S->rows[i], 0, S->wCols, APPLY_ATTR(' '));
}


static void scroll_up(struct emuState *S, int top, int btm, int count)
{
    // FIXME: this is unoptimized.
    assert(count > 0);
    while(count--) {
        struct termRow *movingRow = S->rows[btm];
        for(int i = btm; i > top; i--)
            S->rows[i] = S->rows[i - 1];
        S->rows[top] = movingRow;
        row_fill(movingRow, 0, S->wCols, APPLY_ATTR(' '));
    }
}


static void term_index(struct emuState *S, int count)
{
    if(unlikely(count == 0)) return;

    if(likely(count > 0)) {
        // positive scroll - scroll down
        int dist = S->bScroll - S->cRow;
        if(dist >= count) {
            S->cRow += count;
        } else {
            S->cRow = S->bScroll;
            scroll_down(S, S->tScroll, S->bScroll, count - dist);
        }
    } else {
        count = -count; // scroll up
        int dist = S->cRow - S->tScroll;
        if(dist >= count)
            S->cRow -= count;
        else {
            S->cRow = S->tScroll;
            scroll_up(S, S->tScroll, S->bScroll, count - dist);
        }
    }
}


static void term_cursorHorz(struct emuState *S, int count)
{
    // XXX: reverse wraparound?
    S->cCol += count;
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
    S->wrapnext = 0;
}


#ifdef DEBUG
static const char * safeHex(uint32_t ch)
{
    // Remember, kids, always practice safe hex.
    static char buf[64];
    if(ch < 0x20) {
        snprintf(buf, sizeof(buf), "^%c", ch + 0x40);
    } else if(ch > 0x20 && ch < 0x7f) {
        snprintf(buf, sizeof(buf), "%c", ch);
    } else if(ch <= 0xff) {
        snprintf(buf, sizeof(buf), "%02x", ch);
    } else if(ch <= 0xffff) {
        int lo = ch & 0xff, hi = (ch >> 8) & 0xff;
        if(lo > 0x20 && lo < 0x7f && hi > 0x20 && hi < 0x7f) {
            snprintf(buf, sizeof(buf), "%c%c", hi, lo);
        } else {
            snprintf(buf, sizeof(buf), "%04x", ch);
        }
    }
    return buf;
}
#endif


//////////////////////////////////////////////////////////////////////////////


static void do_modes(struct emuState *S, int flag)
{
    for(int i = 0; i < S->paramPtr; i++) {
        int mode = (S->intermed << 8) | S->params[i];
        switch(mode) {
#ifdef DEBUG
            default:
                printf("unhandled mode %x/%d\n", S->intermed, mode);
#endif
        }
    }
}


static void do_csi_sgr(struct emuState *S)
{
    for(int i = 0; i < S->paramPtr; i++) {
        switch(S->params[i]) {
            case 0:
                S->cursorAttr = 0;
                break;
            case 1: // bold / increased intensity
                S->cursorAttr |= ATTR_BOLD;
                break;
            // case 2: faint
            case 3: // italic
                S->cursorAttr |= ATTR_ITALIC;
                break;
            case 4: // single underline
                S->cursorAttr |= ATTR_UNDERLINE;
                break;
            case 5: // slow blink
            case 6: // fast blink (!)
                S->cursorAttr |= ATTR_BLINK;
                break;
            case 7: // negative image
                S->cursorAttr |= ATTR_REVERSE;
                break;
            // case 8: invisible
            case 9: // crossed out
                S->cursorAttr |= ATTR_STRIKE;
                break;
            // case 10 ... 19: alt fonts
            // case 20: fraktur?!
            case 21: // double underline
                S->cursorAttr |= ATTR_UNDERLINE;
                break;
            case 22: // unbold / unfaint
                S->cursorAttr &= ~ATTR_BOLD;
                break;
            case 23: // un-italic / unFraktur
                S->cursorAttr &= ~ATTR_ITALIC;
                break;
            case 24: // un-underline ("derline"?)
                S->cursorAttr &= ~ATTR_UNDERLINE;
                break;
            case 25: // un-blink
                S->cursorAttr &= ~ATTR_BLINK;
                break;
            // case 26: un-used
            case 27: // un-reverse
                S->cursorAttr &= ~ATTR_REVERSE;
                break;
            //case 28: un-invisible
            case 29: // un-crosssed-out
                S->cursorAttr &= ~ATTR_STRIKE;
                break;
            case 30 ... 37: // foreground colors
                S->cursorAttr &= ~ATTR_FG_MASK;
                S->cursorAttr |= ATTR_CUSTFG | (S->params[i] - 30);
                break;
            //case 38: extended FG, FIXME
            case 39: // default FG
                S->cursorAttr &= ~(ATTR_FG_MASK | ATTR_CUSTFG);
                break;
            case 40 ... 47: // background colors
                S->cursorAttr &= ~ATTR_BG_MASK;
                S->cursorAttr |= ATTR_CUSTBG | (S->params[i] - 40) << 8;
                break;
            // case 48: extended BG, FIXME
            case 49: // default FG
                S->cursorAttr &= ~(ATTR_BG_MASK | ATTR_CUSTBG);
                break;
            case 90 ... 97: // bright foreground colors (not in ECMA048)
                S->cursorAttr &= ~ATTR_FG_MASK;
                S->cursorAttr |= ATTR_CUSTFG | (8 + S->params[i] - 90);
                break;
            case 100 ... 107: // bright background colors (not in ECMA048)
                S->cursorAttr &= ~ATTR_BG_MASK;
                S->cursorAttr |= ATTR_CUSTBG | (8 + S->params[i] - 100) << 8;
                break;
#ifdef DEBUG
            default:
                printf("unhandled SGR %d\n", S->params[i]);
#endif
        }
    }
}


static void dispatch_ctrl(struct emuState *S, uint8_t ch)
{
    switch(ch) {
        case 0x07: // BEL
            TerminalEmulator_bell(S);
            break;

        case 0x08: // BS
            term_cursorHorz(S, -1);
            break;

        case 0x09: // HT
            // FIXME
            break;

        case 0x0A: // NL
        case 0x0B: // VT
        case 0x0C: // NP
            term_index(S, 1);
            if(S->flags & MODE_NEWLINE)
                S->cCol = 0;
            S->wrapnext = 0;
            break;

        case 0x0D: // CR
            S->cCol = 0;
            S->wrapnext = 0;
            break;

        // case 0x1B: ESC is handled upstream

#ifdef DEBUG
        default:
            printf("Unknown control char %s\n", safeHex(ch));
#endif
    }
}


static void dispatch_esc(struct emuState *S, uint8_t lastch)
{
    uint32_t ch = (S->intermed << 8) | lastch;

    switch(ch) {
        case '7': // DECSC (Save Cursor)
            S->saveRow  = S->cRow;
            S->saveCol  = S->cCol;
            S->saveAttr = S->cursorAttr;
            break;

        case '8': // DECRC (Restore Cursor)
            S->cRow = S->saveRow;
            S->cCol = S->saveCol;
            S->cursorAttr = S->saveAttr;
            CAP_MAX(S->cRow, S->wRows - 1);
            CAP_MAX(S->cCol, S->wCols - 1);
            break;
            
        case 'D': // IND
            term_index(S, 1);
            break;

        case 'E': // NEL
            term_index(S, 1);
            S->cCol = 0;
            break;

        case 'M': // RI
            term_index(S, -1);
            break;

        // case '[': CSI is handled upstream

        case CHAR2('#', '8'): // DECALN
            for(int i = 0; i < S->wRows; i++)
                row_fill(S->rows[i], 0, S->wCols, APPLY_ATTR('E'));
            break;

#ifdef DEBUG
        default:
            printf("unknown ESC %s\n", safeHex(ch));
            break;
#endif
    }
}


static void dispatch_csi(struct emuState *S, uint8_t lastch)
{
    uint32_t ch = (S->intermed << 8) | lastch;
    int from, to, top, btm, row; // common var names
    switch(ch) {
        case 'A': // CUU
            row = S->cRow;
            S->cRow -= DEFAULT(S->params[0], 1);
            CAP_MIN(S->cRow, (row < S->tScroll) ? 0 : S->tScroll);
            S->wrapnext = 0;
            break;

        case 'B': // CUD
            row = S->cRow;
            S->cRow += DEFAULT(S->params[0], 1);
            CAP_MAX(S->cRow, (row > S->bScroll) ? S->wRows - 1 : S->bScroll);
            S->wrapnext = 0;
            break;

        case 'C': // CUF
            term_cursorHorz(S, DEFAULT(S->params[0], 1));
            break;

        case 'D': // CUB
            term_cursorHorz(S, -DEFAULT(S->params[0], 1));
            break;

        case 'E': // CNL
            term_index(S, DEFAULT(S->params[0], 1));
            S->cCol = 0;
            break;

        case 'F': // CPL
            term_index(S, -DEFAULT(S->params[0], 1));
            S->cCol = 0;
            break;

        case 'G': // CHA
            S->cCol = S->params[0];
            CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
            break;

        case 'H': // CUP
        case 'f': // HVP
            S->cRow = DEFAULT(S->params[0], 1) - 1;
            S->cCol = DEFAULT(S->params[1], 1) - 1;
            if(S->flags & MODE_ORIGIN) {
                S->cRow += S->tScroll;
                CAP_MAX(S->cRow, S->bScroll);
            } else
                CAP_MIN_MAX(S->cRow, 0, S->wRows - 1);
            CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
            S->wrapnext = 0;
            break;

        case 'J': // ED
            from = 0;
            to = S->wRows - 1;
            switch(S->params[0]) {
                default:
                case 0:
                    from = S->cRow + 1;
                    break;
                case 1:
                    to = S->cRow - 1;
                    break;
                case 2:
                    break;
            }
            for(int i = from; i <= to; i++)
                row_fill(S->rows[i], 0, S->wCols, APPLY_ATTR(' '));
            // FALL THROUGH (intentionally -- ED erases partial lines too)

        case 'K': // EL
            from = 0;
            to = S->wCols - 1;
            switch(S->params[0]) {
                default:
                case 0:
                    from = S->cCol;
                    break;
                case 1:
                    to = S->cCol;
                    break;
                case 2:
                    break;
            }
            row_fill(S->rows[S->cRow], from, to - from + 1, APPLY_ATTR(' '));
            break;

        case 'c': // DA
            TerminalEmulator_writeStr(S, "\e[?1;2c");
            break;

        case 'h':
            do_modes(S, 1);
            break;

        case 'l':
            do_modes(S, 0);
            break;

        case 'm': // SGR
            do_csi_sgr(S);
            break;

        case 'r': // DECSTBM
            top = DEFAULT(S->params[0], 1);
            btm = DEFAULT(S->params[1], 65535);
            CAP_MAX(btm, S->wRows);
            if(btm > top) { // FIXME: is this correct?
                S->tScroll = top - 1;
                S->bScroll = btm - 1;
                S->cRow = (S->flags & MODE_ORIGIN) ? S->tScroll : 0;
                S->cRow = 0;
            }
            break;

#ifdef DEBUG
        default:
            printf("unhandled CSI");
            if(S->paramPtr > 0)
                printf(" %d", S->params[0]);
            for(int i = 1; i < S->paramPtr; i++)
                printf("; %d", S->params[i]);
            printf(" %s\n", safeHex(ch));
#endif
    }
}


//////////////////////////////////////////////////////////////////////////////


void emu_ops_init(struct emuState *S, int rows, int cols)
{
    for(int i = 0; i < 258; i++)
        S->palette[i] = (default_colormap[i] << 8) | 0xff;
    
    S->wRows = rows;
    S->wCols = cols;
    
    S->tScroll = 0;
    S->bScroll = rows - 1;
    
    S->flags = MODE_WRAPAROUND;
    S->cursorAttr = 0;

    size_t rowSize = sizeof(struct termRow) + sizeof(uint64_t) * cols;
    S->rowBase = calloc(rows, rowSize);
    S->rows = calloc(rows, sizeof(struct termRow *));
    
    for(int i = 0; i < rows; i++) {
        S->rows[i] = S->rowBase + i * rowSize;
        row_fill(S->rows[i], 0, cols, APPLY_ATTR(' '));
    }        
}


void emu_ops_free(struct emuState *S)
{
    free(S->rowBase);
    free(S->rows);
}


void emu_ops_resize(struct emuState *S, int rows, int cols)
{
    // FIXME: this is the simplest and worst possible resize impl (erase all)
    
    free(S->rowBase);
    free(S->rows);
    
    size_t rowSize = sizeof(struct termRow) + sizeof(uint64_t) * cols;
    S->rowBase = calloc(rows, rowSize);
    S->rows = calloc(rows, sizeof(struct termRow *));
    
    for(int i = 0; i < rows; i++)
        S->rows[i] = S->rowBase + i * rowSize;
    
    S->cRow = S->cCol = 0;
    
    S->wRows = rows;
    S->wCols = cols;
    
    S->tScroll = 0;
    S->bScroll = rows - 1;
}


void emu_ops_text(struct emuState *S, const uint8_t *bytes, size_t len)
{
    // FIXME: accelerate
    for(int i = 0; i < len; i++) {
        if(unlikely(S->wrapnext)) {
            if(S->flags & MODE_WRAPAROUND) {
                S->rows[S->cRow]->flags |= TERMROW_WRAPPED;
                term_index(S, 1);
                S->cCol = 0;
            }
            S->wrapnext = 0;
        }

        S->rows[S->cRow]->chars[S->cCol++] = APPLY_ATTR(bytes[i]);
        S->rows[S->cRow]->flags |= TERMROW_DIRTY;

        if(unlikely(S->cCol == S->wCols)) {
            S->cCol = S->wCols - 1;
            S->wrapnext = 1;
        }
    }
}


void emu_ops_exec(struct emuState *S, enum emuOpType type, uint8_t final)
{
    switch(type) {
        case EMUOP_CTRL:
            dispatch_ctrl(S, final);
            break;

        case EMUOP_ESC:
            dispatch_esc(S, final);
            break;

        case EMUOP_CSI:
            dispatch_csi(S, final);
            break;

        default:
            abort();
    }
}


