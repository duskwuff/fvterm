#include <stdlib.h>

#include "TerminalEmulator.h"
#include "DefaultColors.h"

void TerminalEmulator_init(struct emulatorState *S, int rows, int cols)
{
    S->rowBase = calloc(rows, sizeof(struct termRow));
    S->rows = calloc(rows, sizeof(struct termRow *));

    for(int i = 0; i < rows; i++) {
        S->rows[i] = &S->rowBase[i];
    }

    for(int i = 0; i < 258; i++)
        S->palette[i] = (default_colormap[i] << 8) | 0xff;

    S->wRows = rows;
    S->wCols = cols;
}


void TerminalEmulator_handleResize(struct emulatorState *S, int rows, int cols)
{
    abort();
}

void TerminalEmulator_free(struct emulatorState *S)
{
    // FIXME
}

void TerminalEmulator_run(struct emulatorState *S, const uint8_t *bytes, size_t len)
{
    // FIXME
}
