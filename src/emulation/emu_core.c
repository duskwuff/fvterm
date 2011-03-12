#include "emu_utils.h"

enum emuCoreState {
    ST_GROUND,
    ST_ESC,
    ST_CSI,
};

void emu_core_init(struct emuState *S, int rows, int cols)
{
    S->coreState = ST_GROUND;
    emu_ops_init(S, rows, cols);
}

void emu_core_resize(struct emuState *S, int rows, int cols)
{
    emu_ops_resize(S, rows, cols);
}

void emu_core_free(struct emuState *S)
{
    emu_ops_free(S);
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
                emu_ops_exec(S, EMUOP_CTRL, ch);
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
                    emu_ops_exec(S, EMUOP_ESC, ch);
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
                    emu_ops_exec(S, EMUOP_CSI, ch);
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
