/* TerminalView */

@class TerminalWindow;
@class TerminalFont;

#define TERMINALVIEW_HSPACE 2
#define TERMINALVIEW_VSPACE 2

@interface TerminalView : NSView
{
    IBOutlet TerminalWindow *parent;
    int redrawCounter;
    BOOL running, redrawPending;
@public
    TerminalFont *font;
}
- (void)resizeForTerminal;
- (void)triggerRedrawWithDataLength:(int)length;

+ (void)releaseBitmaps:(void **)bmap;
@end

// vim: set syn=objc:
