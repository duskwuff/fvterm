#include "emu_core.h"
#include "emu_ops.h"

#include <assert.h>
#include <string.h>


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
        emu_row_fill(S->rows[i], 0, S->wCols, EMPTY_FIELD);
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
        emu_row_fill(movingRow, 0, S->wCols, EMPTY_FIELD);
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
    term_index(S, p1);
    S->cCol = 0;
}

static void do_CPL(struct emuState *S)
{
    int p1 = GETARG(S, 0, 1);
    term_index(S, -p1);
    S->cCol = 0;
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
    if(S->flags & MODE_ORIGIN) {
        S->cRow += S->tScroll;
        CAP_MIN_MAX(S->cRow, 0, S->bScroll);
    } else {
        CAP_MIN_MAX(S->cRow, 0, S->wRows - 1);
    }
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
    S->wrapnext = 0;
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
        S->cRow = 0;
    }
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
    // TODO
}

static void do_IND(struct emuState *S)
{
    term_index(S, 1);
}

static void do_NEL(struct emuState *S)
{
    term_index(S, 1);
    S->cCol = 0;
    S->wrapnext = 0;
}

static void do_NL(struct emuState *S)
{
    term_index(S, 1);
    if(S->flags & MODE_NEWLINE)
        S->cCol = 0;
    S->wrapnext = 0;
}

static void do_RI(struct emuState *S)
{
    term_index(S, -1);
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

static void do_modes(struct emuState *S, int flag)
{
    for(int i = 0; i < S->paramPtr; i++) {
        switch(PACK3(S->priv, S->intermed, S->params[i])) {
            case PACK3('?', 0, 3): // DECCOLM (132/80 switch)
                emu_core_resize(S, S->wRows, flag ? 132 : 80);
                break;
                
            case PACK3('?', 0, 6): // DECOM (origin mode)
                APPLY_FLAG(MODE_ORIGIN, flag);
                break;
                
#ifdef DEBUG
            default:
                printf("unhandled mode %x/%x/%d (flag: %d)\n",
                       S->priv, S->intermed, S->params[i], flag);
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
    uint32_t ch = (S->intermed << 8) | lastch;

    switch(ch) {
            CASE('7', do_DECSC);
            CASE('8', do_DECRC);
            
            CASE('D', do_IND);
            CASE('E', do_NEL);
            CASE('M', do_RI);
            
            CASE2('#', '8', do_DECALN);

#ifdef DEBUG
        default:
            printf("unknown ESC %s\n", safeHex(ch));
            break;
#endif
    }
}

void emu_ops_do_csi(struct emuState *S, uint8_t lastch)
{
    uint32_t ch = (S->intermed << 8) | lastch;
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
            
            CASE('c', do_DA);
            CASE('f', do_CUP_HVP);
            CASE('r', do_DECSTBM);
            CASE('m', do_SGR);
            
        case 'h':
            do_modes(S, 1);
            break;

        case 'l':
            do_modes(S, 0);
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
