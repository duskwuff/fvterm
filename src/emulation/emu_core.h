#ifndef _EMU_CORE_H
#define _EMU_CORE_H

#include <stdint.h>
#include <stdlib.h>

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

// Functions defined in emu_core

enum emuOpType {
    EMUOP_CTRL,
    EMUOP_ESC,
    EMUOP_CSI,
    EMUOP_OSC,
};

void emu_core_init(struct emuState *S, int rows, int cols);
void emu_core_resize(struct emuState *S, int rows, int cols);
size_t emu_core_run(struct emuState *S, const uint8_t *bytes, size_t len);
void emu_core_free(struct emuState *S);

void emu_ops_init(struct emuState *S, int rows, int cols);
void emu_ops_resize(struct emuState *S, int rows, int cols);
void emu_ops_text(struct emuState *S, const uint8_t *bytes, size_t len);
void emu_ops_exec(struct emuState *S, enum emuOpType type, uint8_t final);
void emu_ops_free(struct emuState *S);

// Functions to be defined by clients of emu_core

void TerminalEmulator_bell(struct emuState *S);
void TerminalEmulator_setTitle(struct emuState *S, const char *title);
void TerminalEmulator_resize(struct emuState *S, int rows, int cols);
void TerminalEmulator_write(struct emuState *S, char *bytes, size_t len);
void TerminalEmulator_writeStr(struct emuState *S, char *bytes);

#endif // _EMU_CORE_H