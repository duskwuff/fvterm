#import "emu_core.h"

@class TerminalPTY;
@class TerminalView;

@interface TerminalWindow : NSDocument {
    IBOutlet TerminalView *view;
    TerminalPTY *pty;
    NSString *title;
    int lastDragX, lastDragY;
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
