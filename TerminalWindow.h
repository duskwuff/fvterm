#import "TerminalEmulator.h"

@class TerminalPTY;
@class TerminalView;

#define BITMAP_PTRS     2

@interface TerminalWindow : NSDocument {
    IBOutlet TerminalView *view;
    TerminalPTY *pty;
    NSString *title;
@public
    struct emulatorState emuState;
}

- (void)eventKeyInput:(TerminalView *)view event:(NSEvent *)evt;
- (void)eventMouseInput:(TerminalView *)view event:(NSEvent *)evt;

- (void)ptyInput:(TerminalPTY *)pty data:(NSData *)data;
- (void)ptyClosed:(TerminalPTY *)pty;
@end

// vim: set syn=objc:
