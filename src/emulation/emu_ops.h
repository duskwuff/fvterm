#ifndef _EMU_UTILS_H
#define _EMU_UTILS_H

#include "emu_core.h"

#ifndef unlikely
# define unlikely(x) __builtin_expect((x), 0)
#endif

#ifndef likely
# define likely(x) __builtin_expect((x), 1)
#endif

#define GETARG(S, n, def) (S->params[n] == 0 ? (def) : S->params[n])

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

#define APPLY_ATTR(ch) ((((uint64_t) S->cursorAttr) << 32) | (ch))

#define EMPTY_FIELD APPLY_ATTR(0x20)

#define APPLY_FLAG(mask, val) do { \
    if(val) S->flags |= (mask); \
    else S->flags &= ~(mask); \
} while(0)

#define PACK2(c1, c2) ((c1 << 8) | (c2))
#define PACK3(c1, c2, c3) (((c1) << 16) | ((c2) << 8) | (c3))
#define PACK4(c1, c2, c3, c4) (((c1) << 24) | ((c2) << 16) | ((c3) << 8) | (c4))

void emu_term_reset(struct emuState *S);

void emu_row_fill(struct termRow *row, int start, int count, uint64_t value);
void emu_scroll_up(struct emuState *S, int top, int btm, int count);
void emu_scroll_down(struct emuState *S, int top, int btm, int count);
void emu_term_index(struct emuState *S, int count);

void emu_ops_text(struct emuState *S, const uint8_t *bytes, size_t len);
void emu_ops_do_ctrl(struct emuState *S, uint8_t ch);
void emu_ops_do_esc(struct emuState *S, uint8_t ch);
void emu_ops_do_csi(struct emuState *S, uint8_t ch);
void emu_ops_do_osc(struct emuState *S, int op);

#endif // _EMU_UTILS_H