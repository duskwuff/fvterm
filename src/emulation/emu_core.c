#include "emu_core.h"
#include "emu_ops.h"
#include "DefaultColors.h"

#include <assert.h>
#include <string.h>

enum emuCoreState {
    ST_GROUND,
    ST_ESC,
    ST_CSI,
};

void emu_core_init(struct emuState *S, int rows, int cols)
{
    S->coreState = ST_GROUND;

    for(int i = 0; i < 258; i++)
        S->palette[i] = (default_colormap[i] << 8) | 0xff;
    
    S->cRow = S->cCol = S->saveRow = S->saveCol = 0;
    
    S->wRows = rows;
    S->wCols = cols;
    
    S->tScroll = 0;
    S->bScroll = rows - 1;
    
    S->flags = MODE_WRAPAROUND;
    S->cursorAttr = S->saveAttr = 0;

    size_t rowSize = sizeof(struct termRow) + sizeof(uint64_t) * cols;
    S->rowBase = calloc(rows, rowSize);
    S->rows = calloc(rows, sizeof(struct termRow *));
    
    for(int i = 0; i < rows; i++) {
        S->rows[i] = S->rowBase + i * rowSize;
        emu_row_fill(S->rows[i], 0, cols, EMPTY_FIELD);
    }
}

void emu_core_resize(struct emuState *S, int rows, int cols)
{
    // FIXME: this is the simplest and worst possible resize impl (erase all)
    
    free(S->rowBase);
    free(S->rows);
    
    size_t rowSize = sizeof(struct termRow) + sizeof(uint64_t) * cols;
    S->rowBase = calloc(rows, rowSize);
    S->rows = calloc(rows, sizeof(struct termRow *));
    
    for(int i = 0; i < rows; i++) {
        S->rows[i] = S->rowBase + i * rowSize;
        emu_row_fill(S->rows[i], 0, cols, EMPTY_FIELD);
    }
    
    S->cRow = S->cCol = S->saveRow = S->saveCol = 0;
    
    S->wRows = rows;
    S->wCols = cols;
    
    S->tScroll = 0;
    S->bScroll = rows - 1;
    
    TerminalEmulator_resize(S);
}

void emu_core_free(struct emuState *S)
{
    free(S->rowBase);
    free(S->rows);
}

size_t emu_core_run(struct emuState *S, const uint8_t *bytes, size_t len)
{
    enum emuCoreState lstate = S->coreState;
    int first_ground = -1, ground_len = 0;

#define GROUND_FLUSH() do { \
    if(first_ground >= 0) { \
        emu_ops_text(S, bytes + first_ground, ground_len); \
        first_ground = -1; \
    } \
} while(0)

    for(int i = 0; i < len; i++) {
        uint8_t ch = bytes[i];

        if(ch < 0x20) {
            GROUND_FLUSH();
            if(ch == 0x1B) { // State-changing - trap this locally
                S->priv = S->intermed = 0;
                lstate = ST_ESC;
            } else {
                emu_ops_do_ctrl(S, ch);
            }
            continue;
        }

        if(lstate != ST_GROUND)
            GROUND_FLUSH();

        switch(lstate) {
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
                } else if(ch == '[') { // CSI introducer!
                    lstate = ST_CSI;
                    S->paramPtr = S->paramVal = 0;
                    S->priv = S->intermed = 0;
                    bzero(S->params, sizeof(S->params));
                } else {
                    lstate = ST_GROUND;
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
                    lstate = ST_GROUND;
                }
                break;
        }
    }

    GROUND_FLUSH();

    S->coreState = lstate;
    return len;

#undef GROUND_FLUSH
}


//////////////////////////////////////////////////////////////////////////////
//
// Utilities used by both emu_core and emu_ops

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
