#import <util.h>
#import <setjmp.h>

@class TerminalPTY;
@class TerminalView;

#define BITMAP_PTRS     2

struct termRow {
    void *bitmaps[BITMAP_PTRS];
    BOOL dirty, wrapped;
    uint64_t chars[];
};

@interface TerminalWindow : NSDocument {
    IBOutlet TerminalView *view;
    TerminalPTY *pty;
    NSString *title;
    struct EmulationState *S;
    BOOL cursorKeyMode, invertMode;
    
@public // stuff that TerminalView needs to see
    struct winsize size;
    struct termRow **rows, *rowBase;
    int cRow, cCol;
    BOOL cVisible;
    uint32_t palette[256 + 2];
}

- (void)setWinSize:(struct winsize)newSize;
- (struct winsize)winSize;

- (void)eventKeyInput:(TerminalView *)view event:(NSEvent *)evt;
- (void)eventMouseInput:(TerminalView *)view event:(NSEvent *)evt;

- (void)ptyClosed:(TerminalPTY *)pty;
@end

// vim: set syn=objc: