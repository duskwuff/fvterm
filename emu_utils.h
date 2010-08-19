#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "emu_core.h"

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

enum emuOpType {
    EMUOP_CTRL,
    EMUOP_ESC,
    EMUOP_CSI,
    EMUOP_OSC,
};

void emu_core_init(struct emuState *S, int rows, int cols);
void emu_core_resize(struct emuState *S, int rows, int cols);
int  emu_core_run(struct emuState *S, const uint8_t *bytes, size_t len);
void emu_core_free(struct emuState *S);

void emu_ops_init(struct emuState *S, int rows, int cols);
void emu_ops_resize(struct emuState *S, int rows, int cols);
void emu_ops_text(struct emuState *S, const uint8_t *bytes, size_t len);
void emu_ops_exec(struct emuState *S, enum emuOpType type, uint8_t final);
void emu_ops_free(struct emuState *S);

