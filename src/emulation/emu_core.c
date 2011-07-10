#include "emu_core.h"
#include "emu_ops.h"
#include "DefaultColors.h"

#include <assert.h>
#include <string.h>

enum emuCoreState {
    ST_GROUND,
    ST_ESC,
    ST_CSI,
    ST_OSC,
};

void emu_term_reset(struct emuState *S)
{
    S->state = ST_GROUND;
    S->utf8state = 0;

    for(int i = 0; i < 258; i++)
        S->palette[i] = (default_colormap[i] << 8) | 0xff;

    S->cRow = S->cCol = S->saveRow = S->saveCol = 0;

    S->tScroll = 0;
    S->bScroll = S->wRows - 1;

    S->flags = MODE_WRAPAROUND | MODE_SHOWCURSOR | MODE_ALLOW_DECCOLM;
    S->cursorAttr = S->saveAttr = 0;

    for(int i = 0; i < S->wRows; i++)
        emu_row_fill(S->rows[i], 0, S->wCols, EMPTY_FIELD);

    for(int i = 0; i < S->wCols; i++) {
        if(i % 8 == 7)
            S->colFlags[i] |= COLFLAG_TAB;
    }
}

static void allocBackBuffers(struct emuState *S)
{
    size_t rowSize = sizeof(struct termRow) + sizeof(uint64_t) * S->wCols;
    S->rowBase = calloc(S->wRows, rowSize);
    S->rows = calloc(S->wRows, sizeof(struct termRow *));
    S->colFlags = calloc(S->wCols, sizeof(uint8_t));

    for(int i = 0; i < S->wRows; i++)
        S->rows[i] = S->rowBase + i * rowSize;
}

void emu_core_init(struct emuState *S, int rows, int cols)
{
    S->wRows = rows;
    S->wCols = cols;

    allocBackBuffers(S);
    emu_term_reset(S);
}

void emu_core_resize(struct emuState *S, int rows, int cols)
{
    struct termRow **old_rows = S->rows;
    uint8_t *old_colFlags = S->colFlags;
    void *old_rowBase = S->rowBase;

    int old_wRows = S->wRows, old_wCols = S->wCols;

    S->wRows = rows;
    S->wCols = cols;

    allocBackBuffers(S);
    emu_term_reset(S);

    // FIXME: Improve this.
    for(int r = 0; r < rows && r < old_wRows; r++) {
        for(int c = 0; c < cols && c < old_wCols; c++)
            S->rows[r]->chars[c] = old_rows[r]->chars[c];
        S->rows[r]->flags = TERMROW_DIRTY;
    }

    for(int r = 0; r < old_wRows; r++)
        TerminalEmulator_freeRowBitmaps(old_rows[r]);
    free(old_rows);
    free(old_colFlags);
    free(old_rowBase);

    TerminalEmulator_resize(S);
}

void emu_core_free(struct emuState *S)
{
    for(int r = 0; r < S->wRows; r++)
        TerminalEmulator_freeRowBitmaps(S->rows[r]);
    free(S->rowBase);
    free(S->rows);
    free(S->colFlags);
}

size_t emu_core_run(struct emuState *S, const uint8_t *bytes, size_t len)
{
    int first_ground = -1, ground_len = 0;

#define GROUND_FLUSH() do { \
    if(first_ground >= 0) { \
        emu_ops_text(S, bytes + first_ground, ground_len); \
        first_ground = -1; \
    } \
} while(0)

#define UTF8_FLUSH() emu_ops_text(S, NULL, 0)

    for(int i = 0; i < len; i++) {
        uint8_t ch = bytes[i];

        if(ch < 0x20 && S->state != ST_OSC) {
            GROUND_FLUSH();
            UTF8_FLUSH();
            if(ch == 0x1B) { // State-changing - trap this locally
                S->priv = S->intermed = 0;
                S->state = ST_ESC;
            } else {
                emu_ops_do_ctrl(S, ch);
            }
            continue;
        }

        if(ch >= 0x80 && ch < 0xA0) { // C1 control characters
            emu_ops_do_c1(S, ch);
            S->state = ST_GROUND; // FIXME: check this
            continue;
        }

        if(S->state != ST_GROUND) {
            GROUND_FLUSH();
            UTF8_FLUSH();
        }

        switch(S->state) {
            case ST_GROUND:
                if(first_ground < 0) {
                    first_ground = i;
                    ground_len = 1;
                } else {
                    ground_len++;
                }
                break;

            case ST_ESC:
                if(ch < 0x30) {
                    S->intermed = (S->intermed << 8) | ch;
                    if(S->intermed >= 0xffff)
                        S->intermed = 0xffff;
                } else {
                    S->state = ST_GROUND;
                    emu_ops_do_esc(S, ch);
                }
                break;

            case ST_CSI:
                if(ch < 0x30) { // intermediate
                    S->intermed = (S->intermed << 8) | ch;
                    if(S->intermed >= 0xffff)
                        S->intermed = 0xffff;
                } else if(ch < 0x3A) { // digit
                    S->paramVal = 10 * S->paramVal + (ch - 0x30);
                    CAP_MAX(S->paramVal, 16383);
                } else if(ch == 0x3A) { // colon (???)
                    // FIXME
                } else if(ch == 0x3B) { // semicolon
                    if(S->paramPtr < MAX_PARAMS)
                        S->params[S->paramPtr++] = S->paramVal;
                    S->paramVal = 0;
                } else if(ch < 0x40) { // private
                    S->priv = (S->priv << 8) | ch;
                    if(S->priv >= 0xffff)
                        S->priv = 0xffff;
                } else { // dispatch
                    if(S->paramPtr < MAX_PARAMS)
                        S->params[S->paramPtr++] = S->paramVal;
                    emu_ops_do_csi(S, ch);
                    S->state = ST_GROUND;
                }
                break;

            case ST_OSC:
                // OSC is heavily underspecified in ECMA48. I've come up with
                // some rules here that mimic xterm's behavior.
                if(S->priv == 0) {
                    if(ch >= 0x30 && ch < 0x3A) {
                        S->paramVal = 10 * S->paramVal + (ch - 0x30);
                    } else if(ch == 0x3b) {
                        S->priv = 1;
                    } else {
                        S->state = ST_GROUND; // eh, whatever
                    }
                    break;
                }

                if(ch == 0x07 || ch == 0x9C || (ch == 0x5C && S->priv == 2)) {
                    // ECMA48 specifies ST (ESC 0x5C or 0x9C), vt100 uses BEL.
                    // We allow both.
                    emu_ops_do_osc(S, S->paramVal);
                    S->state = ST_GROUND;
                }

                if((ch >= 0x20 && ch < 0x7f) || (ch >= 0xa0)) {
                    // ECMA48 allows for "00/08 to 00/13 and 02/00 to 07/14",
                    // but I've tweaked the conditions a bit to allow UTF8 text
                    // and disallow control characters.
                    if(S->paramPtr < sizeof(S->oscBuf) - 1)
                        S->oscBuf[S->paramPtr++] = ch;
                } else {
                    // Invalid characters terminate OSC, so that you don't get
                    // stuck in OSC mode forever.
                    S->state = ST_GROUND;
                }
                break;
        }
    }

    GROUND_FLUSH();

    return len;

#undef GROUND_FLUSH
}

void emu_core_start_csi(struct emuState *S)
{
    S->state = ST_CSI;
    S->paramPtr = S->paramVal = 0;
    S->priv = S->intermed = 0;
    bzero(S->params, sizeof(S->params));
}

void emu_core_start_osc(struct emuState *S)
{
    S->state = ST_OSC;
    S->paramPtr = S->paramVal = 0;
    S->priv = S->intermed = 0;
    bzero(S->oscBuf, sizeof(S->oscBuf));
}


//////////////////////////////////////////////////////////////////////////////
//
// Buffer-manipulating utilities

void emu_row_fill(struct termRow *row, int start, int count, uint64_t value)
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


void emu_scroll_down(struct emuState *S, int top, int btm, int count)
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


void emu_scroll_up(struct emuState *S, int top, int btm, int count)
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


void emu_term_index(struct emuState *S, int count)
{
    if(unlikely(count == 0)) return;

    if(likely(count > 0)) {
        // positive scroll - scroll down
        int dist = S->bScroll - S->cRow;
        if(dist >= count) {
            S->cRow += count;
        } else {
            S->cRow = S->bScroll;
            emu_scroll_down(S, S->tScroll, S->bScroll, count - dist);
        }
    } else {
        count = -count; // scroll up
        int dist = S->cRow - S->tScroll;
        if(dist >= count)
            S->cRow -= count;
        else {
            S->cRow = S->tScroll;
            emu_scroll_up(S, S->tScroll, S->bScroll, count - dist);
        }
    }
}
