@class TerminalWindow;

@interface TerminalPTY : NSObject {
    BOOL alive;
    NSFileHandle *term;
    TerminalWindow *parent;
    pid_t pid;
}

- (id)initWithParent:(TerminalWindow *)tw winsize:(struct winsize)ws;
- (void)setSize:(struct winsize)ws;
- (void)writeData:(NSData *)dat;
- (BOOL)alive;

@end

// vim: set syn=objc:
