#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "libfvterm.h"
#include "TerminalEmulator.h"


//////////////////////////////////////////////////////////////////////////////


struct fvterm * fvterm_init(int rows, int cols)
{
    struct fvterm *self = malloc(sizeof(struct fvterm));
    self->state = malloc(sizeof(struct emulatorState));
    bzero(self->state, sizeof(struct emulatorState));
    TerminalEmulator_init(self->state, rows, cols);
    self->state->parent = self;
    return self;
}


void fvterm_free(struct fvterm *self)
{
    TerminalEmulator_free(self->state);
    free(self->state);
    free(self);
}


void fvterm_write(struct fvterm *self, const uint8_t *data, size_t len)
{
    TerminalEmulator_run(self->state, data, len);
}


void fvterm_setsize(struct fvterm *self, int rows, int cols)
{
    TerminalEmulator_handleResize(self->state, rows, cols);
}


void fvterm_getsize(struct fvterm *self, int *rows, int *cols)
{
    if(rows) *rows = self->state->wRows;
    if(cols) *cols = self->state->wCols;
}


void fvterm_getcursor(struct fvterm *self, int *row, int *col)
{
    if(row) *row = self->state->cRow;
    if(col) *col = self->state->cCol;
}


int fvterm_getrowflags(struct fvterm *self, int row)
{
    if(row < 0 || row >= self->state->wRows) return ~0;
    return self->state->rows[row]->flags;
}


uint64_t fvterm_getglyph(struct fvterm *self, int row, int col)
{
    if(row < 0 || row >= self->state->wRows) return ~0;
    if(col < 0 || col >= self->state->wCols) return ~0;
    return self->state->rows[row]->chars[col];
}


//////////////////////////////////////////////////////////////////////////////


void TerminalEmulator_writeStr(struct emulatorState *S, char *bytes)
{
    TerminalEmulator_write(S, bytes, strlen(bytes));
}

void TerminalEmulator_write(struct emulatorState *S, char *bytes, size_t len)
{
    struct fvterm *self = S->parent;
    if(self->outputp + len > sizeof(self->output)) return;
    memcpy(&self->output[self->outputp], bytes, len);
    self->outputp += len;
}

void TerminalEmulator_bell(struct emulatorState *S)
{
    struct fvterm *self = S->parent;
    self->beeps++;
}

void TerminalEmulator_setTitle(struct emulatorState *S, const char *title)
{
    struct fvterm *self = S->parent;
    strncpy(self->title, title, sizeof(self->title));
    self->title[sizeof(self->title) - 1] = 0;
}

void TerminalEmulator_resize(struct emulatorState *S, int rows, int cols)
{
    // nada?
}

