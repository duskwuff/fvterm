#ifndef _EMU_CORE_H
#define _EMU_CORE_H

#include <stdint.h>
#include <stdlib.h>

#define __MAX(x,y) (((x) > (y)) ? (x) : (y))
#define __MIN(x,y) (((x) < (y)) ? (x) : (y))

#define _BIT(n) (1UL<<(n))
#define _BIT_RANGE(a,b) ((_BIT(__MAX(a,b)+1) - 1) & ~(_BIT(__MIN(a,b)) - 1))

#define BITMAP_PTRS 2

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
    uint8_t *colFlags;

    uint32_t cursorAttr, saveAttr;

    int wrapnext, tScroll, bScroll;
    uint64_t flags;

    int state, paramPtr, paramVal, priv, intermed;
    int utf8state;
    uint8_t utf8buf[4];

    // We'll only be using one of these at a time, so they share storage
    union {
        int params[MAX_PARAMS];
        char oscBuf[512];
    };
};

#define TERMROW_DIRTY       _BIT(0)
#define TERMROW_WRAPPED     _BIT(1)

#define ATTR_FG_MASK        _BIT_RANGE(0,  7)
#define ATTR_BG_MASK        _BIT_RANGE(8,  15)
#define ATTR_MD_MASK        _BIT_RANGE(16, 31)

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

#define MODE_MOUSE_DOWN     _BIT(58)
#define MODE_MOUSE_UP       _BIT(59)
#define MODE_MOUSE_MODS     _BIT(59) /* yes, same as _UP */
#define MODE_MOUSE_DRAG     _BIT(60)
#define MODE_MOUSE_HILITE   _BIT(61)
#define MODE_MOUSE_MOTION   _BIT(62)

#define MODE_MOUSE_X10      (MODE_MOUSE_DOWN)
#define MODE_MOUSE_1000     (MODE_MOUSE_X10 | MODE_MOUSE_UP | MODE_MOUSE_MODS)
#define MODE_MOUSE_1001     (MODE_MOUSE_1000 | MODE_MOUSE_HILITE)
#define MODE_MOUSE_1002     (MODE_MOUSE_1000 | MODE_MOUSE_DRAG)
#define MODE_MOUSE_1003     (MODE_MOUSE_1002 | MODE_MOUSE_MOTION)

#define MODE_MOUSE_MASK     _BIT_RANGE(58, 62)

#define COLFLAG_TAB         _BIT(0)

#define PAL_DEFAULT_FG      (256+0)
#define PAL_DEFAULT_BG      (256+1)

void emu_core_init(struct emuState *S, int rows, int cols);
void emu_core_resize(struct emuState *S, int rows, int cols);
size_t emu_core_run(struct emuState *S, const uint8_t *bytes, size_t len);
void emu_core_free(struct emuState *S);

void emu_core_start_csi(struct emuState *S);
void emu_core_start_osc(struct emuState *S);

// Functions to be defined by clients of emu_core

void TerminalEmulator_bell(struct emuState *S);
void TerminalEmulator_setTitle(struct emuState *S, const char *title);
void TerminalEmulator_resize(struct emuState *S);
void TerminalEmulator_write(struct emuState *S, char *bytes, size_t len);
void TerminalEmulator_writeStr(struct emuState *S, char *bytes);
void TerminalEmulator_freeRowBitmaps(struct termRow *r);

#endif // _EMU_CORE_H
