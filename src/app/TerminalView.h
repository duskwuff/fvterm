/* TerminalView */

@class TerminalWindow;
@class TerminalFont;

@interface TerminalView : NSView
{
    IBOutlet TerminalWindow *parent;
    int redrawCounter;
    BOOL running, redrawPending;
    TerminalFont *font;
}
- (void)resizeForTerminal;
- (void)triggerRedrawWithDataLength:(int)length;

+ (void)releaseBitmaps:(void **)bmap;
@end

// vim: set syn=objc:
