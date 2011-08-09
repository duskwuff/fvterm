#import "TerminalWindow.h"
#import "TerminalView.h"
#import "TerminalPTY.h"
#import "TerminalFont.h"

#import "ConsoleKeyMappings.h"


@implementation TerminalWindow


- (NSString *)windowNibName
{
    return @"TerminalWindow";
}


- (NSString *)displayName
{
    if([pty alive])
        return title;
    else
        return [title stringByAppendingString:@" (closed)"];
}


- (void)windowControllerDidLoadNib:(NSWindowController *)controller
{
    [super windowControllerDidLoadNib:controller];

    title = @"Terminal";

    NSUserDefaults *dflt = [NSUserDefaults standardUserDefaults];
    int cols = (int) [dflt integerForKey:@"initCols"];
    if(cols <= 0) cols = 80;
    int rows = (int) [dflt integerForKey:@"initRows"];
    if(rows <= 0) rows = 24;

    state.parent = self;
    emu_core_init(&state, rows, cols);

    pty = [[TerminalPTY alloc] initWithParent:self rows:rows cols:cols];

    [view resizeForTerminal];
}


- (void)dealloc
{
    emu_core_free(&state);

    [title release];
    [pty release];
    [super dealloc];
}


#pragma mark - Event handling


- (void)eventKeyInput:(TerminalView *)view event:(NSEvent *)ev
{
    uint8_t buf[64], ctr = 0;
    uint64_t flags = [ev modifierFlags];
    if(flags & NSCommandKeyMask) return; // we shouldn't have seen that

    NSString *rawKeys = [ev charactersIgnoringModifiers];
    NSString *modKeys = [ev characters];
    NSUInteger rkLen = [rawKeys length];
    NSUInteger mkLen = [modKeys length];
    if(rkLen == 0 || mkLen == 0) return; // um...

    uint16_t ch    = [modKeys characterAtIndex:0];
    uint16_t rawch = [rawKeys characterAtIndex:0];

    int vtShift = !!(flags & NSShiftKeyMask);
    int vtCtrl  = !!(flags & NSControlKeyMask);
    int vtAlt   = !!(flags & NSAlternateKeyMask);

    int vtMode = 0;
    if(vtShift) vtMode |= 1;
    if(vtAlt)   vtMode |= 2;
    if(vtCtrl)  vtMode |= 4;

    if(ch >= 0xf700 && ch < 0xf8ff) { // function keys
        int idx = ch - 0xf700;
        if(idx >= sizeof(consoleKeyMappings) / sizeof(consoleKeyMappings[0]))
            return; // not a key we recognize!
        struct consoleKeyMap *ckm = &consoleKeyMappings[idx];
        if(ckm->type == CKM_CURS && vtMode == 0)
            ctr = snprintf((char *) buf, sizeof(buf), "\e%c%c",
                           (state.flags & MODE_CURSORKEYS) ? 'O' : '[',
                           ckm->content);
        else if(ckm->type == CKM_PF && vtMode == 0)
            ctr = snprintf((char *) buf, sizeof(buf), "\eO%c", ckm->content);
        else if(ckm->type == CKM_PF || ckm->type == CKM_CURS)
            ctr = snprintf((char *) buf, sizeof(buf), "\e[1;%d%c", vtMode + 1, ckm->content);
        else if(ckm->type == CKM_NUM && vtMode == 0)
            ctr = snprintf((char *) buf, sizeof(buf), "\e[%d~", ckm->content);
        else if(ckm->type == CKM_NUM)
            ctr = snprintf((char *) buf, sizeof(buf), "\e[%d;%d~", vtMode + 1, ckm->content);
    } else {
        if(flags & 0x40) vtAlt = 0; // special case for rightopt

        if(vtAlt) {
            buf[ctr++] = '\e';
            ch = rawch; // undo the effects of option (hopefully)
        }
        if(ch < 0x80) {
            buf[ctr++] = ch;
        } else if(ch < 0x800) {
            buf[ctr++] = 0xc0 | (ch >> 6);
            buf[ctr++] = 0x80 | (ch & 0x3f);
        } else {
            buf[ctr++] = 0xe0 | (ch >> 12);
            buf[ctr++] = 0x80 | ((ch >> 6) & 0x3f);
            buf[ctr++] = 0x80 | (ch & 0x3f);
        }
    }

    if(ctr > 0) [pty writeData:[NSData dataWithBytes:buf length:ctr]];
}


- (void)eventMouseInput:(TerminalView *)v event:(NSEvent *)ev
{
    // Translate location
    NSPoint relPt = [v convertPoint:[ev locationInWindow] fromView:nil];
    int x = 1 + (relPt.x - TERMINALVIEW_HSPACE) / view->font->width;
    int y = 1 + (relPt.y - TERMINALVIEW_VSPACE) / view->font->height;

    // xterm's button numbers don't match Apple's, so we translate
    int xBtn;
    switch([ev buttonNumber]) {
        case 0: // left mouse
        default:
            xBtn = 0;
            break;
        case 1: // right mouse
            xBtn = 2;
            break;
        case 2: // middle mouse, most likely
            xBtn = 1;
            break;
    }

    uint8_t buf[64];
    int ctr = 0;
    buf[ctr++] = '\e'; // CSI M
    buf[ctr++] = '[';
    buf[ctr++] = 'M';

    switch([ev type]) {
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
            if(!(state.flags & MODE_MOUSE_DOWN)) return;
            buf[ctr++] = 32 + xBtn;
            buf[ctr++] = 32 + x;
            buf[ctr++] = 32 + y;
            lastDragX = x;
            lastDragY = y;
            break;

        case NSLeftMouseUp:
        case NSRightMouseUp:
        case NSOtherMouseUp:
            if(!(state.flags & MODE_MOUSE_UP)) return;
            buf[ctr++] = 32 + 3; // 3 = release
            buf[ctr++] = 32 + x;
            buf[ctr++] = 32 + y;
            break;

        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSOtherMouseDragged:
            if(!(state.flags & MODE_MOUSE_DRAG)) return;
            if(x == lastDragX && y == lastDragY) return;
            buf[ctr++] = 32 + 32 + xBtn; // yes, we really do add 32 twice
            buf[ctr++] = 32 + x;
            buf[ctr++] = 32 + y;
            lastDragX = x;
            lastDragY = y;
            break;

        case NSScrollWheel:
            if(!(state.flags & MODE_MOUSE_DOWN)) return;
            if(fabs([ev deltaY]) < 0.05) return; // not significant, probably zero
            xBtn = ([ev deltaY] > 0) ? 64 : 65;
            buf[ctr++] = 32 + xBtn;
            buf[ctr++] = 32 + x;
            buf[ctr++] = 32 + y;
            break;

        // FIXME: any other events we need to handle here?

        default:
            return;
    }

    [pty writeData:[NSData dataWithBytes:buf length:ctr]];
}


- (void)viewDidResize:(NSView *)src rows:(int)rows cols:(int)cols;
{
    emu_core_resize(&state, rows, cols);
    [pty setRows:rows cols:cols];
}


- (void)ptyInput:(TerminalPTY *)pty data:(NSData *)data
{
    emu_core_run(&state, [data bytes], [data length]);
    [view setNeedsDisplay:YES];
}


- (void)ptyClosed:(TerminalPTY *)pty
{
    if([[NSUserDefaults standardUserDefaults] boolForKey:@"keepZombies"]) {
        // Poke the controller to make the new title show up
        [[self windowControllers] makeObjectsPerformSelector:
         @selector(synchronizeWindowTitleWithDocumentName)];
    } else {
        // Closin' time
        [self close];
    }
}


#pragma mark - TerminalEmulator callback functions


void TerminalEmulator_bell(struct emuState *S)
{
    NSBeep();
}


void TerminalEmulator_setTitle(struct emuState *S, const char *newTitle)
{
    TerminalWindow *self = S->parent;
    [self->title release];
    self->title = [[NSString stringWithUTF8String:newTitle] retain];
    [[self windowControllers] makeObjectsPerformSelector:
        @selector(synchronizeWindowTitleWithDocumentName)];
}


void TerminalEmulator_resize(struct emuState *S)
{
    TerminalWindow *self = S->parent;
    [self->view resizeForTerminal];
}


void TerminalEmulator_write(struct emuState *S, char *bytes, size_t len)
{
    TerminalWindow *self = S->parent;
    [self->pty writeData:[NSData dataWithBytesNoCopy:bytes length:len
                                  freeWhenDone:NO]];
}


void TerminalEmulator_writeStr(struct emuState *S, char *bytes)
{
    TerminalEmulator_write(S, bytes, strlen(bytes));
}


void TerminalEmulator_freeRowBitmaps(struct termRow *r)
{
    [TerminalView releaseBitmaps:r->bitmaps];
}


@end