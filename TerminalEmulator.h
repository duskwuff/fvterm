#include <stdint.h>
#include <util.h>

#define BITMAP_PTRS 2
#define TERMROW_DIRTY   0x0001
#define TERMROW_WRAPPED 0x0002

#define MAX_PARAMS 16

struct termRow {
    void *bitmaps[BITMAP_PTRS];
    int flags;
    uint64_t chars[];
};

struct emulatorState {
    void *parent;

    int cRow, cCol, saveRow, saveCol;
    uint32_t palette[256 + 2];
    int wRows, wCols;
    struct termRow **rows;
    void *rowBase;

    enum emuState {
        ST_GROUND,
        ST_ESC,
        //ST_ESC_INT - folded into ST_ESC
        ST_CSI,
        ST_CSI_INT,
        ST_CSI_PARM,
        ST_CSI_IGNORE,
    } state;
    uint32_t cursorAttr, saveAttr;

    int wrapnext, tScroll, bScroll;
    uint64_t flags;

    int params[MAX_PARAMS], paramPtr, paramVal, priv, intermed;
};

#define ATTR_FG_MASK    0x000000FFUL
#define ATTR_BG_MASK    0x0000FF00UL
#define ATTR_FG_SHIFT   0
#define ATTR_BG_SHIFT   8
#define ATTR_MD_MASK    0xFFFF0000UL

#define ATTR_DEFAULT    0x00000007UL

//#define ATTR_BOLD       0x00010008UL
#define ATTR_BOLD       0x00010000UL
#define ATTR_UNDERLINE  0x00020000UL
#define ATTR_BLINK      0x00040000UL
#define ATTR_REVERSE    0x00080000UL
#define ATTR_CUSTFG     0x00100000UL
#define ATTR_CUSTBG     0x00200000UL

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

#define PACK_2(x,y)     ( ((x) <<  8) | (y) )
#define PACK_3(x,y,z)   ( ((x) << 16) | ((y) <<  8) | (z))
#define PACK_4(x,y,z,a) ( ((x) << 24) | ((y) << 16) | ((z) << 8) | (a))

void TerminalEmulator_init(struct emulatorState *S, int rows, int cols);
void TerminalEmulator_free(struct emulatorState *S);
int TerminalEmulator_run(struct emulatorState *S, const uint8_t *bytes, size_t len);
void TerminalEmulator_handleResize(struct emulatorState *S, int rows, int cols);

void TerminalEmulator_setTitle(struct emulatorState *S, const char *title);
void TerminalEmulator_resize(struct emulatorState *S, int rows, int cols);

