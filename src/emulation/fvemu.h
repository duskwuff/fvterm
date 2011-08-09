#ifndef _FVEMU_H
#define _FVEMU_H

#include <stdint.h>
#include <stdlib.h>

#define BITMAP_PTRS 2
#define MAX_PARAMS 16

struct termRow {
    void *bitmaps[BITMAP_PTRS];
    int flags;
    uint64_t chars[];
};

enum emuCoreState {
    ST_GROUND,
    ST_ESC,
    ST_CSI,
    ST_OSC,
};

struct emuState {
    void *parent;

    int cRow, cCol;
    uint32_t palette[256 + 2];
    int wRows, wCols;
    struct termRow **rows;
    void *rowBase;
    uint8_t *colFlags;

    int wrapnext, tScroll, bScroll;
    uint32_t cursorAttr;
    uint64_t flags;

    int state, paramPtr, paramVal;
    uint8_t intermed, vt52Hack;

    int utf8state;
    uint8_t utf8buf[4];

    uint8_t charset, charsets[4];

    int saveRow, saveCol;
    uint32_t saveAttr;
    uint64_t saveFlags;
    uint8_t saveCharset, saveCharsets[4];

    // We'll only be using one of these at a time, so they share storage
    union {
        int params[MAX_PARAMS];
        char oscBuf[512];
    };
};

#define _BIT(n) (1UL<<(n))

#define TERMROW_DIRTY       _BIT(0)
#define TERMROW_WRAPPED     _BIT(1)

#define ATTR_FG_MASK        (0xFFUL   << 0)
#define ATTR_BG_MASK        (0xFFUL   << 8)
#define ATTR_MD_MASK        (0xFFFFUL << 16)

#define ATTR_BOLD           _BIT(16)
#define ATTR_UNDERLINE      _BIT(17)
#define ATTR_BLINK          _BIT(18)
#define ATTR_REVERSE        _BIT(19)
#define ATTR_ITALIC         _BIT(20)
#define ATTR_STRIKE         _BIT(21)
#define ATTR_CUSTFG         _BIT(22)
#define ATTR_CUSTBG         _BIT(23)

#define MODE_WRAPAROUND     _BIT(0)
#define MODE_REVWRAP        _BIT(1)
#define MODE_ORIGIN         _BIT(2)
#define MODE_LINEFEED       _BIT(3)
#define MODE_INSERT         _BIT(4)
#define MODE_SOFTSCROLL     _BIT(5)
#define MODE_NEWLINE        _BIT(6)
#define MODE_CURSORKEYS     _BIT(7)
#define MODE_INVERT         _BIT(8)
#define MODE_SHOWCURSOR     _BIT(9)
#define MODE_ALLOW_DECCOLM  _BIT(10)
#define MODE_VT52           _BIT(11)

#define MODE_MOUSE_DOWN     _BIT(59)
#define MODE_MOUSE_UP       _BIT(60)
#define MODE_MOUSE_MODS     _BIT(60) /* yes, same as _UP */
#define MODE_MOUSE_DRAG     _BIT(61)
#define MODE_MOUSE_HILITE   _BIT(62)
#define MODE_MOUSE_MOTION   _BIT(63)

#define MODE_MOUSE_X10      (MODE_MOUSE_DOWN)
#define MODE_MOUSE_1000     (MODE_MOUSE_X10 | MODE_MOUSE_UP | MODE_MOUSE_MODS)
#define MODE_MOUSE_1001     (MODE_MOUSE_1000 | MODE_MOUSE_HILITE)
#define MODE_MOUSE_1002     (MODE_MOUSE_1000 | MODE_MOUSE_DRAG)
#define MODE_MOUSE_1003     (MODE_MOUSE_1002 | MODE_MOUSE_MOTION)

#define MODE_MOUSE_MASK     (MODE_MOUSE_DOWN | MODE_MOUSE_UP | \
                             MODE_MOUSE_MODS | MODE_MOUSE_DRAG | \
                             MODE_MOUSE_HILITE | MODE_MOUSE_MOTION)

#define COLFLAG_TAB         _BIT(0)

#define PAL_DEFAULT_FG      (256+0)
#define PAL_DEFAULT_BG      (256+1)

// Functions exported by fvemu

void emu_core_init(struct emuState *S, int rows, int cols);
void emu_core_resize(struct emuState *S, int rows, int cols);
size_t emu_core_run(struct emuState *S, const uint8_t *bytes, size_t len);
void emu_core_free(struct emuState *S);

// Functions imported by fvemu

void TerminalEmulator_bell(struct emuState *S);
void TerminalEmulator_setTitle(struct emuState *S, const char *title);
void TerminalEmulator_resize(struct emuState *S);
void TerminalEmulator_write(struct emuState *S, char *bytes, size_t len);
void TerminalEmulator_writeStr(struct emuState *S, char *bytes);
void TerminalEmulator_freeRowBitmaps(struct termRow *r);

#endif // _FVEMU_H