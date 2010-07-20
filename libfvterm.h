#include <stdint.h>
#include <unistd.h>

struct fvterm {
    struct emulatorState *state;
    char output[1024], title[256];
    int beeps, outputp;
};

struct fvterm * fvterm_init(int rows, int cols);
void fvterm_free(struct fvterm *self);

void fvterm_write(struct fvterm *self, const uint8_t *data, size_t len);
void fvterm_setsize(struct fvterm *self, int rows, int cols);
void fvterm_getsize(struct fvterm *self, int *rows, int *cols);
void fvterm_getcursor(struct fvterm *self, int *row, int *col);
int fvterm_getrowflags(struct fvterm *self, int row);
uint64_t fvterm_getglyph(struct fvterm *self, int row, int col);
