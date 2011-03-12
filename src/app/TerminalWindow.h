#import "emu_core.h"

@class TerminalPTY;
@class TerminalView;

#define BITMAP_PTRS     2

@interface TerminalWindow : NSDocument {
    IBOutlet TerminalView *view;
    TerminalPTY *pty;
    NSString *title;
@public
    struct emuState state;
}

- (void)eventKeyInput:(TerminalView *)view event:(NSEvent *)evt;
- (void)eventMouseInput:(TerminalView *)view event:(NSEvent *)evt;
- (void)viewDidResize:(NSView *)src rows:(int)rows cols:(int)cols;

- (void)ptyInput:(TerminalPTY *)pty data:(NSData *)data;
- (void)ptyClosed:(TerminalPTY *)pty;
@end

// vim: set syn=objc:
