#import "TerminalWindow.h"
#import "TerminalView.h"
#import "TerminalPTY.h"

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

    int rows = 24, cols = 80; // XXX

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


- (void)eventKeyInput:(TerminalView *)view event:(NSEvent *)ev
{
    NSString *chars = [ev charactersIgnoringModifiers];
    if(![chars length]) return;
    unichar ch = [chars characterAtIndex:0];

    NSUInteger flags = [ev modifierFlags];

    if(flags & NSCommandKeyMask) return; // leave that shit alone

    if(ch >= 0xF700 && ch < 0xF700 + (sizeof(consoleKeyMappings) / sizeof(*consoleKeyMappings))) {
        struct ConsoleKeyMap ckm = consoleKeyMappings[ch - 0xF700];
        char output[64] = "";
        int mstate = (((flags & NSShiftKeyMask) ? 1 : 0) |
                      ((flags & NSAlternateKeyMask) ? 2 : 0) |
                      ((flags & NSControlKeyMask) ? 4 : 0));

        switch(ckm.type) {
            case CKM_IGNORE:
                NSBeep();
                break;
            case CKM_CURS: // Cursor keys (nothing else!) get modified by cursorKeyMode
                if(/*!cursorKeyMode &&*/ !mstate) {
                    sprintf(output, "\e[%c", ckm.content);
                    break;
                }
                // fall through
            case CKM_CSI:
                if(mstate)
                    sprintf(output, "\e[1;%d%c", mstate + 1, ckm.content);
                else
                    sprintf(output, "\eO%c", ckm.content);
                break;
            case CKM_NUM:
                if(mstate)
                    sprintf(output, "\e[%d;%d~", ckm.content, mstate + 1);
                else
                    sprintf(output, "\e[%d~", ckm.content);
                break;
        }
        if(output[0])
            [pty writeData:[NSData dataWithBytes:output length:strlen(output)]];

    } else if(ch == 0x19) {
        // XXX: need to explain wtf this is.
        [pty writeData:[@"\e[Z" dataUsingEncoding:NSUTF8StringEncoding]];

    } else {
        unsigned char output[16];
        int p = 0;

        if(flags & NSAlternateKeyMask) output[p++] = 0x1B; // esc+char
        if(flags & NSControlKeyMask) ch &= 0x1F; // control-ify

        // fast and dirty UTF-8 conversion
        if(ch < 0x80)
            output[p++] = ch;
        else if(ch < 0x800) {
            output[p++] = 0xc0 | (ch >> 6);
            output[p++] = 0x80 | (ch & 0x3f);
        } else { /* XXX: non-BMP characters get screwed up due to UTF-16 encoding */
                 /* (OTOH, who's about to type non-BMP chars?) */
            output[p++] = 0xe0 | (ch >> 12);
            output[p++] = 0x80 | ((ch >> 6) & 0x3f);
            output[p++] = 0x80 | (ch & 0x3f);
        }

        [pty writeData:[NSData dataWithBytes:output length:p]];
    }
}


- (void)eventMouseInput:(TerminalView *)view event:(NSEvent *)ev
{
    // FIXME
}


- (void)viewDidResize:(NSView *)src rows:(int)rows cols:(int)cols;
{
    emu_core_resize(&state, rows, cols);
}


- (void)ptyInput:(TerminalPTY *)pty data:(NSData *)data
{
    size_t n = emu_core_run(&state, [data bytes], [data length]);
    [view triggerRedrawWithDataLength:(int) n];
}


- (void)ptyClosed:(TerminalPTY *)pty
{
    // Poke the controller to make the new title show up
    [[self windowControllers] makeObjectsPerformSelector:
        @selector(synchronizeWindowTitleWithDocumentName)];
}


//////////////////////////////////////////////////////////////////////////////


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

@end
