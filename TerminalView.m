#import "Prefix.h"

#import "TerminalView.h"
#import "TerminalPTY.h"
#import "TerminalWindow.h"
#import "TerminalWindow_Emulation.h"
#import "TerminalFont.h"

#define HSPACE 2
#define VSPACE 2

static CGColorSpaceRef cspace = nil;

@implementation TerminalView

- (id)_init
{
    running = NO;

    font = [[TerminalFont alloc] initWithFile:[[NSBundle mainBundle] pathForResource:@"fixed13"
                                                                              ofType:@"vtf"]];
    
    if(cspace == nil)
        cspace = CGColorSpaceCreateDeviceRGB();

    return self;
}

- (id)initWithFrame:(NSRect)frame
{
    if(!(self = [super initWithFrame:frame])) return nil;
    return [self _init];
}

- (id)initWithCoder:(NSCoder *)coder
{
    if(!(self = [super initWithCoder:coder])) return nil;
    return [self _init];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [[NSRunLoop mainRunLoop] cancelPerformSelector:@selector(_delayedRedraw:) target:self argument:nil];
    [super dealloc];
}


#pragma mark Event handling

- (void)keyDown:(NSEvent *)ev
{
    [parent eventKeyInput:self event:ev];
}

- (void)mouseDown:(NSEvent *)ev
{
    [parent eventMouseInput:self event:ev];
}

- (void)mouseDragged:(NSEvent *)ev
{
    [parent eventMouseInput:self event:ev];
}

- (void)mouseUp:(NSEvent *)ev
{
    [parent eventMouseInput:self event:ev];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}


#pragma mark Rendering

uint32_t *loadFontPage(NSString *fontName, int pageNum, char pageMode, int charWidth, int charHeight)
{
    NSLog(@"Loading font %@ page %d mode %c (%dx%d)", fontName, pageNum, pageMode, charWidth, charHeight);
    
    NSData *fontData = [NSData dataWithContentsOfFile:
                        [[NSBundle mainBundle] pathForResource:
                         [NSString stringWithFormat:@"%03x-%c", pageNum, pageMode]
                                                        ofType:@"raw"
                                                   inDirectory:
                         [@"fonts" stringByAppendingPathComponent:fontName]]];
    
    if(!fontData || [fontData length] != charWidth * charHeight * 32)
        return NULL;

    uint32_t *buf = malloc(charWidth * charHeight * 32 * 8 * 4), *bp = buf;
    const uint8_t *fp = [fontData bytes];
    int bit = 0;
    
    // image format conversion
    for(int row = 0; row < 8; row++)
        for(int crow = 0; crow < charHeight; crow++) {
            for(int col = 0; col < 32; col++)
                for(int ccol = 0; ccol < charWidth; ccol++) {
                    *bp++ = (*fp & (0x80 >> bit++)) ? ~0 : 0;
                    if(bit == 8) {
                        fp++;
                        bit = 0;
                    }
                }
            /* sync bitstream at end of rows */
            if(bit) {
                fp++;
                bit = 0;
            }
        }
    
    return buf;
}

void render(TerminalView *view, struct termRow *row)
{
    uint32_t *plt = view->parent->palette;
    
    TerminalFont *font = view->font;
    int charHeight = font->height;
    int charWidth = font->width;
    int cols = view->parent->size.ws_col;
    
    size_t rowLen = charWidth * cols * sizeof(uint32_t);

    if(row->bitmaps[0] == NULL)
        row->bitmaps[0] = malloc(rowLen * charHeight);

    uint32_t *bmap = row->bitmaps[0];
    
    for(int lrow = 0; lrow < charHeight; lrow++)
        for(int col = 0; col < cols; col++) {
            uint64_t ch = row->chars[col];
            unichar charGlyph = ch & 65535;
            int fontPage = charGlyph >> 8;
            int fontRow = (charGlyph >> 5) & 7;
            int fontCol =  charGlyph & 31;
            
            uint32_t charAttr = ch >> 32;
            int charFG = PAL_DEFAULT_FG, charBG = PAL_DEFAULT_BG;
            if(charAttr & ATTR_CUSTFG)
                charFG = (charAttr & ATTR_FG_MASK) >> ATTR_FG_SHIFT;
            if(charAttr & ATTR_CUSTBG)
                charBG = (charAttr & ATTR_BG_MASK) >> ATTR_BG_SHIFT;
            if(charAttr & ATTR_BOLD) fontPage += 256;
            
            // reverse video = swap fg/bg
            if(charAttr & ATTR_REVERSE) {
                int tmp = charFG;
                charFG = charBG;
                charBG = tmp;
            }
            
            uint8_t *src = font->unpackedPages[fontPage];
            
            if(!src) {
                NSLog(@"Unpacking page %d", fontPage);
                if(![font unpackPage:fontPage]) {
                    NSLog(@"No page for char %x (page %d)!", charGlyph, fontPage);
                    charGlyph = 0;
                    fontPage = 0;
                    [font unpackPage:0];
                }
                
                if(!(src = font->unpackedPages[fontPage])) {
                    NSLog(@"Couldn't load page 0, ugh");
                    abort();
                }
            }
            
            src += font->width * (fontCol + FVFONT_CHARS_WIDE * ((fontRow + 1) * font->height - (lrow + 1)));
            
            for(int i = 0; i < charWidth; i++) {
                *bmap++ = src[i] ? plt[charFG] : plt[charBG];
            }
        }
    
    CGDataProviderRef provider = CGDataProviderCreateWithData(nil, row->bitmaps[0], rowLen * charHeight, nil);
    
    if(row->bitmaps[1])
        CGImageRelease(row->bitmaps[1]);
    
    CGImageRef img = CGImageCreate(cols * charWidth, charHeight,
                                   8, 32, rowLen, 
                                   cspace, kCGBitmapByteOrder32Host,
                                   provider, nil, NO,
                                   kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    row->bitmaps[1] = img;
    
    row->dirty = NO;
}

+ (void)releaseBitmaps:(void **)bmaps
{
    if(bmaps[0]) free(bmaps[0]);
    if(bmaps[1]) CGImageRelease(bmaps[1]);
}

- (void)drawRect:(NSRect)rect
{
    CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];

    CGContextSetGrayFillColor(ctx, 0.133, 1.0); // XXX: constant??
    CGContextFillRect(ctx, NSRectToCGRect([self frame]));
    
    int width  = parent->size.ws_col * font->width;
    CGRect dstRect = { .origin = { HSPACE, VSPACE }, .size = { width, font->height } };
    
    // Draw rows, rendering as necessary
    for(int r = 0; r < parent->size.ws_row; r++) {
        struct termRow *row = parent->rows[r];
        if(row->dirty)
            render(self, row);
        CGContextDrawImage(ctx, dstRect, row->bitmaps[1]);
        dstRect.origin.y += font->height;
    }
    
    CGRect cursor = { .origin = { HSPACE, VSPACE }, .size = { font->width, font->height } };
    cursor.origin.x += font->width  * parent->cCol;
    cursor.origin.y += font->height * parent->cRow;
    
    CGContextSetRGBFillColor(ctx, 0.0, 1.0, 0.0, 0.7); // XXX: Cursor color shouldn't be constant
    CGContextFillRect(ctx, cursor);
    
    running = YES;
    redrawPending = NO;
    redrawCounter = 0;
}

- (void)resizeForTerminal
{
    int termWidth = parent->size.ws_col * font->width + 2 * HSPACE;
    int termHeight = parent->size.ws_row * font->height + 2 * VSPACE;
    NSRect new_cr = NSMakeRect(0, 0, termWidth, termHeight);

    NSRect old_frame = [[self window] frame];
    NSRect new_frame = [[self window] frameRectForContentRect:new_cr];

    new_frame.origin = old_frame.origin;
    new_frame.origin.y -= new_frame.size.height - old_frame.size.height;

    [[self window] setFrame:new_frame display:YES];
    [[self window] setResizeIncrements:NSMakeSize(font->width, font->height)];

    [self setNeedsDisplay:YES];
}

- (void)triggerRedrawWithDataLength:(int)length
{
    if(redrawPending) return;
    
    redrawCounter += length;

    if(redrawCounter < 64) {
        [self display];
    } else {
        redrawPending = YES;
        [self performSelector:@selector(display)
                   withObject:nil
                   afterDelay:1/30.0
                      inModes:[NSArray arrayWithObjects:
                               NSDefaultRunLoopMode, NSEventTrackingRunLoopMode, nil]];
    }
}

- (void)_gotResized:(NSNotification *)note
{
    (void) note;
    
    NSRect frame = [self frame];
    struct winsize newsize;
    newsize.ws_col = (frame.size.width  - 2 * HSPACE) / font->width;
    newsize.ws_row = (frame.size.height - 2 * VSPACE) / font->height;
    newsize.ws_xpixel = newsize.ws_ypixel = 0;
    
    if(newsize.ws_col != parent->size.ws_col || newsize.ws_row != parent->size.ws_row)
        [parent setWinSize:newsize];
}

- (void)viewDidMoveToWindow
{
    // We're definitely running now, so start observing
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_gotResized:)
                                                 name:NSViewFrameDidChangeNotification
                                               object:self];
}

@end

// vim: set syn=objc:
