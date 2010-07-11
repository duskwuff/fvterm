#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "TerminalEmulator.h"
#include "DefaultColors.h"


//////////////////////////////////////////////////////////////////////////////


#ifndef unlikely
# define unlikely(x) __builtin_expect((x), 0)
#endif

#ifndef likely
# define likely(x) __builtin_expect((x), 1)
#endif

#define DEFAULT(n, def) ((n) == 0 ? (def) : (n))

#define CAP_MIN(val, vmin) do { \
    if((val) < (vmin)) val = vmin; \
} while(0)

#define CAP_MAX(val, vmax) do { \
    if((val) > (vmax)) val = vmax; \
} while(0)

#define CAP_MIN_MAX(val, vmin, vmax) do { \
    CAP_MIN(val, vmin); \
    CAP_MAX(val, vmax); \
} while(0)

#define ATTR_PACK(ch, attr) (((uint64_t) (attr) << 32) | ((uint32_t) ch))
#define APPLY_FLAG(mask) do { \
    if(val) S->flags |= (mask); \
    else S->flags &= ~(mask); \
} while(0)

#define CHAR2(c1, c2) ((c1 << 8) | (c2))


//////////////////////////////////////////////////////////////////////////////


void TerminalEmulator_init(struct emulatorState *S, int rows, int cols)
{
    size_t rowSize = sizeof(struct termRow) + sizeof(uint64_t) * cols;
    S->rowBase = calloc(rows, rowSize);
    S->rows = calloc(rows, sizeof(struct termRow *));

    for(int i = 0; i < rows; i++)
        S->rows[i] = S->rowBase + i * rowSize;

    for(int i = 0; i < 258; i++)
        S->palette[i] = (default_colormap[i] << 8) | 0xff;

    S->wRows = rows;
    S->wCols = cols;

    S->tScroll = 0;
    S->bScroll = rows - 1;

    S->flags = MODE_WRAPAROUND;
    S->cursorAttr = 0;
}


void TerminalEmulator_handleResize(struct emulatorState *S, int rows, int cols)
{
    abort();
}


void TerminalEmulator_free(struct emulatorState *S)
{
    // FIXME
}


//////////////////////////////////////////////////////////////////////////////


static void row_fill(struct termRow *row, int start, int count, uint64_t value)
{
    // memset_pattern8 is highly optimized on x86 :)
    memset_pattern8(&row->chars[start], &value, count * 8);
    row->flags = TERMROW_DIRTY;
}


static void scroll_down(struct emulatorState *S, int top, int btm, int count)
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
        row_fill(S->rows[i], 0, S->wCols,
                 ATTR_PACK(' ', S->cursorAttr));
}


static void scroll_up(struct emulatorState *S, int top, int btm, int count)
{
    // FIXME: this is unoptimized.
    assert(count > 0);
    while(count--) {
        struct termRow *movingRow = S->rows[btm];
        for(int i = btm; i > top; i--)
            S->rows[i] = S->rows[i - 1];
        S->rows[top] = movingRow;
        row_fill(movingRow, 0, S->wCols,
                 ATTR_PACK(' ', S->cursorAttr));
    }
}


static void term_index(struct emulatorState *S, int count)
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


static void term_cursorHorz(struct emulatorState *S, int count)
{
    // XXX: reverse wraparound?
    S->cCol += count;
    CAP_MIN_MAX(S->cCol, 0, S->wCols - 1);
    S->wrapnext = 0;
}


static void output_char(struct emulatorState *S, uint32_t uch) {
    if(unlikely(S->wrapnext)) {
        if(S->flags & MODE_WRAPAROUND) {
            S->rows[S->cRow]->flags |= TERMROW_WRAPPED;
            term_index(S, 1);
            S->cCol = 0;
        }
        S->wrapnext = 0;
    }

    S->rows[S->cRow]->chars[S->cCol++] = ATTR_PACK(uch, S->cursorAttr);
    S->rows[S->cRow]->flags |= TERMROW_DIRTY;

    if(unlikely(S->cCol == S->wCols)) {
        S->cCol = S->wCols - 1;
        S->wrapnext = 1;
    }
}


//////////////////////////////////////////////////////////////////////////////


void act_clear(struct emulatorState *S)
{
    S->paramPtr = S->paramVal = 0;
    S->priv = S->intermed = 0;
    bzero(S->params, sizeof(S->params));
}


void dispatch_esc(struct emulatorState *S, uint8_t lastch)
{
    uint32_t ch = (S->intermed << 8) | lastch;
    if(ch < 0x30) { // intermediate
        S->intermed = ch;
        CAP_MAX(S->intermed, 0xffff);
        S->state = ST_ESC; // more!
        return;
    }

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

        case '[': // CSI
            S->state = ST_CSI;
            break;

        case CHAR2('#', '8'): // DECALN
            for(int i = 0; i < S->wRows; i++)
                row_fill(S->rows[i], 0, S->wCols,
                         ATTR_PACK('E', S->cursorAttr));
            break;

        default:
            printf("unknown ESC %02x\n", ch);
            break;
    }
}


void dispatch_csi(struct emulatorState *S, uint8_t lastch)
{
    uint32_t ch = (S->intermed << 8) | lastch;
    int from, to, row; // common var names
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
                row_fill(S->rows[i], 0, S->wCols,
                         ATTR_PACK(' ', S->cursorAttr));
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
            row_fill(S->rows[S->cRow], from, to - from,
                     ATTR_PACK(' ', S->cursorAttr));
            break;

        case 'm':
            for(int i = 0; i < S->paramPtr; i++) {
                switch(S->params[i]) {
                    case 0:
                        S->cursorAttr = 0;
                        break;
                    case 1:
                        S->cursorAttr |= ATTR_BOLD;
                        break;
                    case 4:
                        S->cursorAttr |= ATTR_UNDERLINE;
                        break;
                    case 5:
                        S->cursorAttr |= ATTR_BLINK;
                        break;
                    case 7:
                        S->cursorAttr |= ATTR_REVERSE;
                        break;

                    //case 8: invisible?

                    case 22:
                        S->cursorAttr &= ~ATTR_BOLD;
                        break;
                    case 24:
                        S->cursorAttr &= ~ATTR_UNDERLINE;
                        break;
                    case 25:
                        S->cursorAttr &= ~ATTR_BLINK;
                        break;
                    case 27:
                        S->cursorAttr &= ~ATTR_REVERSE;
                        break;

                    //case 28: invisible?

                    case 30 ... 37:
                        S->cursorAttr &= ~ATTR_FG_MASK;
                        S->cursorAttr |= ATTR_CUSTFG | (S->params[i] - 30);
                        break;

                    //case 38: extended FG
                    
                    case 39: // default FG
                        S->cursorAttr &= ~(ATTR_FG_MASK | ATTR_CUSTFG);
                        break;

                    case 40 ... 47:
                        S->cursorAttr &= ~ATTR_BG_MASK;
                        S->cursorAttr |= ATTR_CUSTBG | (S->params[i] - 40) << 8;
                        break;

                    // case 48: extended BG

                    case 49: // default FG
                        S->cursorAttr &= ~(ATTR_BG_MASK | ATTR_CUSTBG);
                        break;

                    default:
                        printf("unhandled SGR %d\n", S->params[i]);
                }
            }
            break;

        default:
            printf("unhandled ESC CSI ... %02x\n", ch);
            break;
    }
}


int TerminalEmulator_run(struct emulatorState *S, const uint8_t *bytes, size_t len)
{
    for(int i = 0; i < len; i++) {
        uint8_t ch = bytes[i];

        if(ch < 0x20) {
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

                case 0x1B: // ESC
                    S->state = ST_ESC;
                    act_clear(S);
                    break;
            }
            continue;
        }

        switch(S->state) {
            case ST_GROUND:
                output_char(S, ch);
                break;

            case ST_ESC:
                if(ch < 0x30) {
                    if(S->intermed >= 0xffff)
                        S->intermed = 0xffff;
                    else
                        S->intermed = (S->intermed << 8) | ch;
                    break;
                } else {
                    S->state = ST_GROUND;
                    dispatch_esc(S, ch);
                }
                break;

            case ST_CSI:
            case ST_CSI_INT:
            case ST_CSI_PARM:
                if(ch < 0x30) { // intermediate char
                    if(S->intermed >= 0xffff)
                        S->intermed = 0xffff;
                    else
                        S->intermed = (S->intermed << 8) | ch;
                } else if(ch < 0x3A) { // digit
                    if(S->state == ST_CSI_INT) { // invalid!
                        S->state = ST_CSI_IGNORE;
                        break;
                    }
                    CAP_MIN(S->paramVal, 0);
                    S->paramVal = 10 * S->paramVal + (ch - 0x30);
                    CAP_MAX(S->paramVal, 16383);
                    S->state = ST_CSI_PARM;
                } else if(ch == 0x3A) { // colon (invalid)
                    S->state = ST_CSI_IGNORE;
                    break;
                } else if(ch == 0x3B) { // semicolon
                    if(S->state == ST_CSI_INT) {
                        S->state = ST_CSI_IGNORE;
                        break;
                    }
                    if(S->paramPtr < MAX_PARAMS)
                        S->params[S->paramPtr++] = S->paramVal;
                    S->paramVal = 0;
                    S->state = ST_CSI_PARM;
                } else if(ch < 0x40) { // private
                    if(S->state != ST_CSI) {
                        S->state = ST_CSI_IGNORE;
                        break;
                    }
                    S->priv = ch;
                    S->state = ST_CSI_PARM;
                } else {
                    // flush last param
                    if(S->paramPtr < MAX_PARAMS)
                        S->params[S->paramPtr++] = S->paramVal;
                    dispatch_csi(S, ch);
                    S->state = ST_GROUND;
                }
                break;

            case ST_CSI_IGNORE:
                if(ch >= 0x40)
                    S->state = ST_GROUND;
                break;

            default:
                abort(); // WTF
        }
    }

    return len;
}

