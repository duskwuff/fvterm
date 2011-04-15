#include "emu_core.h"
#include "emu_ops.h"

#include <assert.h>
#include <string.h>

#ifdef DEBUG
#include <stdio.h>

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


static void do_BEL(struct emuState *S)
{
    TerminalEmulator_bell(S);
}

static void do_BS(struct emuState *S)
{
    S->cCol -= 1;
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
    S->wrapnext = 0;
}

static void do_CHA(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    S->cCol = p1 - 1;
    S->wrapnext = 0;
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
}

static void do_CNL(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    emu_term_index(S, p1);
    S->cCol = 0;
    S->wrapnext = 0;
}

static void do_CPL(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    emu_term_index(S, -p1);
    S->cCol = 0;
    S->wrapnext = 0;
}

static void do_CR(struct emuState *S)
{
    S->cCol = 0;
    S->wrapnext = 0;
}

static void do_CUB(struct emuState *S)
{
    S->cCol -= GETARG(S, 0, 1);
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
    S->wrapnext = 0;
}

static void do_CUF(struct emuState *S)
{
    S->cCol += GETARG(S, 0, 1);
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
    S->wrapnext = 0;
}

static void do_CUU(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    int row = S->cRow;
    S->cRow -= p1;
    CAP_MIN_MAX(S->cRow, (row < S->tScroll) ? 0 : S->tScroll, S->wCols - 1);
    S->wrapnext = 0;
}

static void do_CUD(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    int row = S->cRow;
    S->cRow += p1;
    CAP_MIN_MAX(S->cRow, 0, (row > S->bScroll) ? S->wRows - 1 : S->bScroll);
    S->wrapnext = 0;
}

static void do_CUP_HVP(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    int p2 = GETARG(S, 1, 1);
    S->cRow = p1 - 1;
    S->cCol = p2 - 1;
    S->wrapnext = 0;
    if(S->flags & MODE_ORIGIN) {
        S->cRow += S->tScroll;
        CAP_MIN_MAX(S->cRow, 0, S->bScroll);
    } else {
        CAP_MIN_MAX(S->cRow, 0, S->wRows - 1);
    }
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
}

static void do_DA(struct emuState *S)
{
    TerminalEmulator_writeStr(S, "\e[?1;2c");
}

static void do_DECALN(struct emuState *S)
{
    for(int i = 0; i < S->wRows; i++)
        emu_row_fill(S->rows[i], 0, S->wCols, APPLY_ATTR('E'));
}

static void do_DECRC(struct emuState *S)
{
    S->cRow = S->saveRow;
    S->cCol = S->saveCol;
    S->wrapnext = 0;
    S->cursorAttr = S->saveAttr;
    CAP_MAX(S->cRow, S->wRows - 1);
    CAP_MAX(S->cCol, S->wCols - 1);
}

static void do_DECSC(struct emuState *S)
{
    S->saveRow  = S->cRow;
    S->saveCol  = S->cCol;
    S->saveAttr = S->cursorAttr;
}

static void do_DECSTBM(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    int p2 = GETARG(S, 1, 65535);
    CAP_MIN(p1, 1);
    CAP_MAX(p2, S->wRows);
    if(p2 > p1) { // FIXME: is this correct?
        S->tScroll = p1 - 1;
        S->bScroll = p2 - 1;
        S->cRow = (S->flags & MODE_ORIGIN) ? S->tScroll : 0;
        S->cCol = 0;
    }
}

static void do_DL(struct emuState *S)
{
    if(S->cRow >= S->tScroll && S->cRow <= S->bScroll)
        emu_scroll_down(S, S->cRow, S->bScroll, GETARG(S, 0, 1));
}

static void do_ED(struct emuState *S)
{
    int p1 = GETARG(S, 0, 0);
    int from = 0;
    int to = S->wRows - 1;
    switch(p1) {
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
        emu_row_fill(S->rows[i], 0, S->wCols, EMPTY_FIELD);
    // ED erases partial lines too...
    if(p1 == 1)
        emu_row_fill(S->rows[S->cRow], 0, S->cCol + 1, EMPTY_FIELD);
    else if(p1 != 2) // 0 or default
        emu_row_fill(S->rows[S->cRow], S->cCol, S->wCols - S->cCol, EMPTY_FIELD);
}

static void do_EL(struct emuState *S)
{
    int from = 0;
    int to = S->wCols - 1;
    switch(GETARG(S, 0, 0)) {
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
    emu_row_fill(S->rows[S->cRow], from, to - from + 1, EMPTY_FIELD);
}

static void do_HT(struct emuState *S)
{
    do {
        S->cCol++;
    } while(!(S->cCol >= S->wCols || S->colFlags[S->cCol] & COLFLAG_TAB));
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
    S->wrapnext = 0;
}

static void do_HTS(struct emuState *S)
{
    S->colFlags[S->cCol] |= COLFLAG_TAB;
}

static void do_IL(struct emuState *S)
{
    if(S->cRow >= S->tScroll && S->cRow <= S->bScroll)
        emu_scroll_up(S, S->cRow, S->bScroll, GETARG(S, 0, 1));
}

static void do_IND(struct emuState *S)
{
    emu_term_index(S, 1);
}

static void do_NEL(struct emuState *S)
{
    emu_term_index(S, 1);
    S->cCol = 0;
    S->wrapnext = 0;
}

static void do_NL(struct emuState *S)
{
    emu_term_index(S, 1);
    if(S->flags & MODE_NEWLINE)
        S->cCol = 0;
    S->wrapnext = 0;
}

static void do_RI(struct emuState *S)
{
    emu_term_index(S, -1);
}

static void do_SGR(struct emuState *S)
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

static void do_TBC(struct emuState *S)
{
    switch(GETARG(S, 0, 0)) {
        case 0: // clear htab here
            S->colFlags[S->cCol] &= ~COLFLAG_TAB;
            break;

        case 1: // clear vtab here (unimpl)
        case 4: // clear all vtabs (unimpl)
            break;
            
        case 2: // clear all htabs on this line
            // Although ECMA048 implies that this is similar to Ps=3, neither
            // xterm nor rxvt implement it that way, and vttest specifically
            // ensures that it does not, describing it as a "no-op"!
            break;
            
        case 3: // clear all htabs
        case 5: // clear all tabs
            for(int i = 0; i < S->wCols; i++)
                S->colFlags[i] &= ~COLFLAG_TAB;
            break;
    }
}

static void do_VPA(struct emuState *S)
{
    S->cRow = GETARG(S, 0, 1) - 1;
    S->wrapnext = 0;
    if(S->flags & MODE_ORIGIN) {
        S->cRow += S->tScroll;
        CAP_MIN_MAX(S->cRow, 0, S->bScroll);
    } else {
        CAP_MIN_MAX(S->cRow, 0, S->wRows - 1);
    }
}

static void do_modes(struct emuState *S, int flag)
{
    for(int i = 0; i < S->paramPtr; i++) {
        switch(PACK3(S->priv, S->intermed, S->params[i])) {
                
            case PACK3(0, 0, 4): // IRM (insert/replace mode)
                APPLY_FLAG(MODE_INSERT, flag);
                break;
                
            case PACK3(0, 0, 20): // LNM (normal linefeed mode)
                APPLY_FLAG(MODE_LINEFEED, flag);
                break;
                
            case PACK3('?', 0, 1): // DECCKM (cursor key mode)
                APPLY_FLAG(MODE_CURSORKEYS, flag);
                break;
                
            case PACK3('?', 0, 2): // DECANM (vt52 mode)
                // FIXME (it's not just a mode, it's a way of life)
                break;
                
            case PACK3('?', 0, 3): // DECCOLM (132/80 switch)
                emu_core_resize(S, S->wRows, flag ? 132 : 80);
                break;
                
            case PACK3('?', 0, 4): // DECSCLM (smooth scrolling)
                // Ignored - we don't implement this feature
                break;
                
            case PACK3('?', 0, 5): // DECSCNM (reverse video)
                APPLY_FLAG(MODE_INVERT, flag);
                for(int i = 0; i < S->wRows; i++)
                    S->rows[i]->flags |= TERMROW_DIRTY; // redraw everything!
                break;
                
            case PACK3('?', 0, 6): // DECOM (origin mode)
                APPLY_FLAG(MODE_ORIGIN, flag);
                // Origin flag homes the cursor when set/reset
                S->cRow = flag ? S->tScroll : 0;
                S->cCol = 0;
                break;
                
            case PACK3('?', 0, 7): // DECAWM (wraparound mode)
                APPLY_FLAG(MODE_WRAPAROUND, flag);
                break;

            case PACK3('?', 0, 8): // DECARM (autorepeat mode)
            case PACK3('?', 0, 9): // DECINLM (interlace)
                // Ignored - neither of these make sense on a software terminal.
                break;
                
            case PACK3('?', 0, 12): // cursor blink
                // TODO
                break;
                
            case PACK3('?', 0, 25): // visible cursor
                APPLY_FLAG(MODE_SHOWCURSOR, flag);
                break;
                
            case PACK3('?', 0, 1000): // various forms of mouse tracking
            case PACK3('?', 0, 1001):
            case PACK3('?', 0, 1002):
            case PACK3('?', 0, 1003):
                // TODO
                break;
                
            case PACK3('?', 0, 1047): // alternate buffer
            case PACK3('?', 0, 1049): // alternate buffer/cursor
                // TODO
                break;
                
#ifdef DEBUG
            default:
                printf("unhandled mode %x/%x/%d (flag: %d)\n",
                       S->priv, S->intermed, S->params[i], flag);
#endif
        }
    }
}

static void do_SM(struct emuState *S)
{
    do_modes(S, 1);
}

static void do_RM(struct emuState *S)
{
    do_modes(S, 0);
}

static void do_dterm_window(struct emuState *S)
{
    int p1 = GETARG(S, 0, 0);
    int p2 = GETARG(S, 1, 0);
    int p3 = GETARG(S, 2, 0);
    switch(p1) {
        case 0: // deiconify
        case 1: // iconify
        case 3: // move
            // not implemented
            break;
            
        case 4: // resize (pixels)
            // XXX
            break;
            
        case 5: // raise
        case 6: // lower
        case 7: // refresh
            // not implemented
            break;
            
        case 8: // resize (text)
            if(p2 >= 1 && p3 >= 1 && p2 <= 999 && p3 <= 999)
                emu_core_resize(S, p2, p3);
            break;
            
        case 9: // zoom
            // XXX
            break;
            
        case 11: // report state
            // XXX
            break;
            
        case 13: // report position
            // XXX
            break;
            
        case 14: // report size (pixels)
            // XXX
            break;
            
        case 18: // report size (chars)
            // XXX
            break;
            
        case 19: // report size II (chars)
            // XXX
            break;
            
        case 20: // report icon label
        case 21: // report title
            // XXX
            break;
            
        default:
            if(p1 >= 24) { // resize to lines (DECSLPP)
                emu_core_resize(S, GETARG(S, 0, 0), S->wCols);
            } else {
#ifdef DEBUG       
                printf("unhandled xterm window manipulation %d\n", p1);
#endif
            }
    }
}

//////////////////////////////////////////////////////////////////////////////


#define CASE(frm, to) case frm: to(S); break
#define CASE2(frm, frm2, to) CASE(PACK2(frm, frm2), to)

void emu_ops_do_ctrl(struct emuState *S, uint8_t ch)
{
    switch(ch) {
            CASE(0x07, do_BEL);
            CASE(0x08, do_BS);
            CASE(0x09, do_HT);
            CASE(0x0A, do_NL);
            CASE(0x0B, do_NL); // VT -> NL
            CASE(0x0C, do_NL); // NP -> NL
            CASE(0x0D, do_CR);

#ifdef DEBUG
        default:
            printf("Unknown control char %s\n", safeHex(ch));
#endif
    }
}


void emu_ops_do_esc(struct emuState *S, uint8_t lastch)
{
    switch(PACK2(S->intermed, lastch)) {
            CASE('7', do_DECSC);
            CASE('8', do_DECRC);
            
            CASE('D', do_IND);
            CASE('E', do_NEL);
            CASE('H', do_HTS);
            CASE('M', do_RI);
            
            CASE2('#', '8', do_DECALN);

#ifdef DEBUG
        default:
            printf("unknown ESC %s\n", safeHex(PACK2(S->intermed, lastch)));
            break;
#endif
    }
}

void emu_ops_do_csi(struct emuState *S, uint8_t lastch)
{
    switch(PACK2(S->intermed, lastch)) {
            CASE('A', do_CUU);
            CASE('B', do_CUD);
            CASE('C', do_CUF);
            CASE('D', do_CUB);
            CASE('E', do_CNL);
            CASE('F', do_CPL);
            CASE('G', do_CHA);
            CASE('H', do_CUP_HVP);
            CASE('J', do_ED);
            CASE('K', do_EL);
            CASE('L', do_IL);
            CASE('M', do_DL);
            
            CASE('c', do_DA);
            CASE('d', do_VPA);
            CASE('f', do_CUP_HVP);
            CASE('g', do_TBC);
            CASE('h', do_SM);
            CASE('l', do_RM);
            CASE('m', do_SGR);
            CASE('r', do_DECSTBM);
            CASE('t', do_dterm_window);
            
            
#ifdef DEBUG
        default:
            printf("unhandled CSI");
            if(S->paramPtr > 0)
                printf(" %d", S->params[0]);
            for(int i = 1; i < S->paramPtr; i++)
                printf("; %d", S->params[i]);
            printf(" %s\n", safeHex(PACK2(S->intermed, lastch)));
#endif
    }
}


void emu_ops_do_osc(struct emuState *S, int op)
{
    switch(op) {
        case 0: // xterm: set icon name and window title
        case 1: // xterm: set icon name
        case 2: // xterm: set window title
            TerminalEmulator_setTitle(S, S->oscBuf);
            break;
            
#ifdef DEBUG
        default:
            printf("unhandled OSC %d; '%s'\n", op, S->oscBuf);
#endif
    }
}

static void do_unichar(struct emuState *S, uint16_t uc)
{
    if(unlikely(S->wrapnext)) {
        if(S->flags & MODE_WRAPAROUND) {
            S->rows[S->cRow]->flags |= TERMROW_WRAPPED;
            emu_term_index(S, 1);
            S->cCol = 0;
        }
        S->wrapnext = 0;
    }
    
    S->rows[S->cRow]->chars[S->cCol++] = APPLY_ATTR(uc);
    S->rows[S->cRow]->flags |= TERMROW_DIRTY;
    
    if(unlikely(S->cCol == S->wCols)) {
        S->cCol = S->wCols - 1;
        S->wrapnext = 1;
    }
}

static void unwind_utf8(struct emuState *S)
{
    switch(S->utf8state) {
        case 1:
        case 2:
        case 4:
            do_unichar(S, S->utf8buf[0]);
            break;
            
        case 3:
        case 5:
            do_unichar(S, S->utf8buf[1]);
            do_unichar(S, S->utf8buf[0]);
            break;
            
        case 6:
            do_unichar(S, S->utf8buf[2]);
            do_unichar(S, S->utf8buf[1]);
            do_unichar(S, S->utf8buf[0]);
            break;
    }
    S->utf8state = 0;
}

void emu_ops_text(struct emuState * restrict S, const uint8_t *bytes, size_t len)
{
    if(len == 0) {
        unwind_utf8(S);
        return;
    }
    
    for(int i = 0; i < len; i++) {
        uint8_t c = bytes[i];
        if(unlikely(S->utf8state > 0)) {
            if(c < 0x80 || c >= 0xc0) {
                unwind_utf8(S);
            } else {
                switch(S->utf8state) {
                    case 1: // process 2/2
                        do_unichar(S, ((S->utf8buf[0] & 0x3f) << 6)
                                   |   (c & 0x3f));
                        S->utf8state = 0;
                        break;
                        
                    case 3: // process 3/3
                        do_unichar(S, ((S->utf8buf[0] & 0x1f) << 12)
                                   |  ((S->utf8buf[1] & 0x3f) << 6)
                                   |   (c & 0x3f));
                        S->utf8state = 0;
                        break;
                        
                    case 6: // process 4/4
                        do_unichar(S, ((S->utf8buf[0] & 0x0f) << 18)
                                   |  ((S->utf8buf[1] & 0x3f) << 12)
                                   |  ((S->utf8buf[2] & 0x3f) << 6)
                                   |   (c & 0x3f));
                        S->utf8state = 0;
                        break;
                        
                    case 2: // process 2/3
                    case 4: // process 2/4
                        S->utf8buf[1] = c;
                        S->utf8state++;
                        break;
                        
                    case 5: // process 3/4
                        S->utf8buf[2] = c;
                        S->utf8state++;
                        break;
                }
                continue;
            }
        }
        if(likely(c < 0x80)) {
            // Just plain ASCII
            do_unichar(S, c);
        } else if(c < 0xc2) {
            // Invalid UTF8, valid ISO8859-1
            do_unichar(S, c);
        } else if(c < 0xe0) {
            // First character of 2-byte sequence
            S->utf8buf[0] = c;
            S->utf8state = 1;
        } else if(c < 0xf0) {
            // First character of 3-byte sequence
            S->utf8buf[0] = c;
            S->utf8state = 2;
        } else if(c < 0xf5) {
            // First character of 4-byte sequence
            S->utf8buf[0] = c;
            S->utf8state = 4;
        } else {
            // Invalid UTF8, valid ISO8859-1
            do_unichar(S, c);
        }
    }
}
