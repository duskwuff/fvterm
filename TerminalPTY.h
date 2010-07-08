@class TerminalWindow;

@interface TerminalPTY : NSObject {
    BOOL alive;
    NSFileHandle *term;
    TerminalWindow *parent;
    pid_t pid;
}

- (id)initWithParent:(TerminalWindow *)tw rows:(int)rows cols:(int)cols;
- (void)setRows:(int)rows cols:(int)cols;
- (void)writeData:(NSData *)dat;
- (BOOL)alive;

@end

// vim: set syn=objc:
