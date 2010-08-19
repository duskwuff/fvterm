#include <stdint.h>

#define BITMAP_PTRS 2
#define TERMROW_DIRTY   0x0001
#define TERMROW_WRAPPED 0x0002

#define MAX_PARAMS 16

struct termRow {
    void *bitmaps[BITMAP_PTRS];
    int flags;
    uint64_t chars[];
};

struct emuState {
    void *parent;

    int cRow, cCol, saveRow, saveCol;
    uint32_t palette[256 + 2];
    int wRows, wCols;
    struct termRow **rows;
    void *rowBase;

    uint32_t cursorAttr, saveAttr;

    int wrapnext, tScroll, bScroll;
    uint64_t flags;

    int coreState, params[MAX_PARAMS], paramPtr, paramVal, priv, intermed;
};

#define ATTR_FG_MASK    0x000000FFUL
#define ATTR_BG_MASK    0x0000FF00UL
#define ATTR_MD_MASK    0xFFFF0000UL

//#define ATTR_BOLD     0x00010008UL
#define ATTR_BOLD       0x00010000UL
#define ATTR_UNDERLINE  0x00020000UL
#define ATTR_BLINK      0x00040000UL
#define ATTR_REVERSE    0x00080000UL
#define ATTR_ITALIC     0x00100000UL
#define ATTR_STRIKE     0x00200000UL
#define ATTR_CUSTFG     0x10000000UL
#define ATTR_CUSTBG     0x20000000UL

#define PAL_DEFAULT_FG 256
#define PAL_DEFAULT_BG 257

#define MODE_WRAPAROUND     0x0001
#define MODE_REVWRAP        0x0002
#define MODE_ORIGIN         0x0004
#define MODE_LINEFEED       0x0008
#define MODE_INSERT         0x0010
#define MODE_SOFTSCROLL     0x0020
#define MODE_NEWLINE        0x0040
#define MODE_CURSORKEYS     0x0080
#define MODE_INVERT         0x0100

void TerminalEmulator_bell(struct emuState *S);
void TerminalEmulator_setTitle(struct emuState *S, const char *title);
void TerminalEmulator_resize(struct emuState *S, int rows, int cols);
void TerminalEmulator_write(struct emuState *S, char *bytes, size_t len);
void TerminalEmulator_writeStr(struct emuState *S, char *bytes);

