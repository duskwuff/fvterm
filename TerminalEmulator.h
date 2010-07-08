#include <stdint.h>
#include <util.h>

#define BITMAP_PTRS 2
#define TERMROW_DIRTY   0x0001
#define TERMROW_WRAPPED 0x0002

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

    int params[16], paramPtr, paramVal, priv, intermed;
};

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

