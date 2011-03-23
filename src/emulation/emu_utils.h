#ifndef _EMU_UTILS_H
#define _EMU_UTILS_H

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

#define APPLY_ATTR(ch) ((((uint64_t) S->cursorAttr) << 32) | (ch))

#define APPLY_FLAG(mask) do { \
if(val) S->flags |= (mask); \
else S->flags &= ~(mask); \
} while(0)

#define CHAR2(c1, c2) ((c1 << 8) | (c2))

#endif // _EMU_UTILS_H