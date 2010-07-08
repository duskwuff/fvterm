#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "TerminalEmulator.h"
#include "DefaultColors.h"

#ifndef unlikely
# define unlikely(x) __builtin_expect((x),0)
#endif

#ifndef likely
# define likely(x) __builtin_expect((x),1)
#endif

#define DEFAULT(n, default) ((n) == 0 ? (default) : (n))
#define CAP_MIN(val, vmin) do { if((val) < (vmin)) val = vmin; } while(0)
#define CAP_MAX(val, vmax) do { if((val) > (vmax)) val = vmax; } while(0)
#define CAP_MIN_MAX(val, vmin, vmax) do { CAP_MIN(val, vmin); CAP_MAX(val, vmax); } while(0)

#define ATTR_PACK(ch, attr) (((uint64_t) (attr) << 32) | ((uint32_t) ch))
#define APPLY_FLAG(mask) if(val) S->flags |= (mask); else S->flags &= ~(mask);

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


void row_fill(struct termRow *row, int start, int count, uint64_t value)
{
    // memset_pattern8 is highly optimized on x86 :)
    memset_pattern8(&row->chars[start], &value, count * 8);
    row->flags = TERMROW_DIRTY;
}

void scroll_down(struct emulatorState *S, int top, int btm, int count)
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


void scroll_up(struct emulatorState *S, int top, int btm, int count)
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

void term_index(struct emulatorState *S, int count)
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


void output_char(struct emulatorState *S, uint32_t uch) {
    if(S->wrapnext) {
        if(S->flags & MODE_WRAPAROUND) {
            S->rows[S->cRow]->flags |= TERMROW_WRAPPED;
            term_index(S, 1);
            S->cCol = 0;
        }
        S->wrapnext = 0;
    }

    S->rows[S->cRow]->chars[S->cCol++] = ATTR_PACK(uch, S->cursorAttr);
    S->rows[S->cRow]->flags |= TERMROW_DIRTY;

    if(S->cCol == S->wCols) {
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


int TerminalEmulator_run(struct emulatorState *S, const uint8_t *bytes, size_t len)
{
    for(int i = 0; i < len; i++) {
        uint8_t ch = bytes[i];

        switch(ch) {
            case 0x00:
                continue;

            case 0x0A: // NL
            case 0x0B: // VT
            case 0x0C: // NP
                term_index(S, 1);
                if(S->flags & MODE_NEWLINE)
                    S->cCol = 0;
                S->wrapnext = 0;
                continue;

            case 0x0D: // CR
                S->cCol = 0;
                S->wrapnext = 0;
                continue;

            case 0x1B: // ESC
                S->state = ST_ESC;
                act_clear(S);
                continue;
        }

        output_char(S, ch);
    }

    return len;
}

