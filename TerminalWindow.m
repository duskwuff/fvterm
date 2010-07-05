#import "TerminalWindow.h"
#import "TerminalView.h"
#import "TerminalPTY.h"
#import "TerminalWindow_Emulation.h"

// NB: Data is defined in the following files...
#import "ConsoleKeyMappings.h"
#import "DefaultColors.h" // defines default_colormap

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

    size.ws_col = 80; // XXX: default size?
    size.ws_row = 24;
    size.ws_xpixel = size.ws_ypixel = 0;

    cRow = cCol = 0;
    cVisible = YES;

    title = @"Terminal";

    [view resizeForTerminal];
    
    [self setWinSize:size];
    pty = [[TerminalPTY alloc] initWithParent:self winsize:size];
    
    for(int i = 0; i < 258; i++) {
        palette[i] = (default_colormap[i] << 8) | 0xff; // Shift in alpha
    }
    
    [self _emulationInit];
}


- (void)dealloc
{
    if(rows) free(rows);
    if(rowBase) free(rowBase);
    
    [title release];
    [pty release];
    [self _emulationFree];
    [super dealloc];
}


- (struct winsize)winSize
{
    return size;
}


- (void)setWinSize:(struct winsize)newSize
{
    if(rows) {
        for(int i = 0; i < size.ws_row; i++) {
            [TerminalView releaseBitmaps:rows[i]->bitmaps];
            free(rows[i]);
        }
        free(rows);
    }
    
    rows = calloc(newSize.ws_row, sizeof(struct termRow *));
    size_t rowSize = sizeof(struct termRow) + sizeof(uint64_t) * newSize.ws_col;
    rowBase = calloc(newSize.ws_row, rowSize);
    
    for(int i = 0; i < newSize.ws_row; i++) {
        rows[i] = ((void *) rowBase) + rowSize * i;
        bzero(rows[i]->bitmaps, sizeof(rows[i]->bitmaps));
        rows[i]->dirty = YES;
        rows[i]->wrapped = NO;
        bzero(rows[i]->chars, sizeof(uint64_t) * newSize.ws_col);
    }
    
    NSLog(@"Setting size to %dx%d", newSize.ws_col, newSize.ws_row);
    
    size = newSize;
    [pty setSize:newSize];
    
    [view resizeForTerminal];
    if(S) [self _emulationResize];
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
                if(!cursorKeyMode && !mstate) {
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
    // ...
}


- (void)ptyClosed:(TerminalPTY *)pty
{
    // Poke the controller to make the new title show up
    [[self windowControllers] makeObjectsPerformSelector:@selector(synchronizeWindowTitleWithDocumentName)];
}


@end
