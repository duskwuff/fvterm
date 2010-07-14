#import "Prefix.h"

#import "TerminalView.h"
#import "TerminalPTY.h"
#import "TerminalWindow.h"
#import "TerminalFont.h"

#define HSPACE 2
#define VSPACE 2

static CGColorSpaceRef cspace = nil;

@implementation TerminalView

- (id)_init
{
    running = NO;

    font = [[TerminalFont alloc] initWithFile:
       [[NSBundle mainBundle] pathForResource:@"fixed-13"
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
    [[NSRunLoop mainRunLoop] cancelPerformSelector:@selector(_delayedRedraw:)
                                            target:self
                                          argument:nil];
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

static const uint8_t * getPage(TerminalFont *font, int page, unichar *glyph) {
    if(font->unpackedPages[page] || [font unpackPage:page])
        return font->unpackedPages[page];

    // try unbold?
    if(page > 255) {
        page &= 255;
        if(font->unpackedPages[page] || [font unpackPage:page])
            return font->unpackedPages[page];
    }

    // try fallback glyph?
    if(glyph) *glyph = 1;
    if(font->unpackedPages[0] || [font unpackPage:0])
        return font->unpackedPages[0];

    NSLog(@"Font %@ has no fallback glyph!", font);
    abort();
}

#define OFFSET_FONT(ptr, lrow, glyph) do { \
    int fontRow = (charGlyph >> 5) & 7; \
    int fontCol = charGlyph & 31; \
    ptr += font->width * ( \
            fontCol + FVFONT_CHARS_WIDE * ( \
                (fontRow + 1) * font->height - (lrow + 1))); \
} while(0)

static void render(TerminalView *view, struct termRow *row)
{
    TerminalFont *font = view->font;
    uint32_t *plt = view->parent->emuState.palette;
    int charHeight = font->height;
    int charWidth = font->width;
    int cols = view->parent->emuState.wCols;

    size_t rowLen = charWidth * cols * sizeof(uint32_t);

    if(row->bitmaps[0] == NULL)
        row->bitmaps[0] = malloc(rowLen * charHeight);

    uint32_t *bmap = row->bitmaps[0];

    for(int lrow = 0; lrow < charHeight; lrow++) {
        for(int col = 0; col < cols; col++) {
            uint64_t ch = row->chars[col];
            unichar charGlyph = ch & 65535;
            int fontPage = charGlyph >> 8;

            uint32_t charAttr = ch >> 32;
            int charFG = PAL_DEFAULT_FG, charBG = PAL_DEFAULT_BG;
            if(charAttr & ATTR_CUSTFG)
                charFG = (charAttr & ATTR_FG_MASK);
            if(charAttr & ATTR_CUSTBG)
                charBG = (charAttr & ATTR_BG_MASK) >> 8;
            if(charAttr & ATTR_BOLD) fontPage += 256;

            // reverse video = swap fg/bg
            if(charAttr & ATTR_REVERSE) {
                int tmp = charFG;
                charFG = charBG;
                charBG = tmp;
            }

            const uint8_t *src = getPage(font, fontPage, &charGlyph);
            OFFSET_FONT(src, lrow, charGlyph);

            for(int i = 0; i < charWidth; i++) {
                *bmap++ = src[i] ? plt[charFG] : plt[charBG];
            }
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

    row->flags &= ~TERMROW_DIRTY;
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

    int width = parent->emuState.wCols * font->width;
    CGRect dstRect = {
        .origin = { HSPACE, VSPACE },
        .size = { width, font->height },
    };

    // Draw rows, rendering as necessary
    for(int r = 0; r < parent->emuState.wRows; r++) {
        struct termRow *row = parent->emuState.rows[r];
        if(row->flags & TERMROW_DIRTY)
            render(self, row);
        CGContextDrawImage(ctx, dstRect, row->bitmaps[1]);
        dstRect.origin.y += font->height;
    }

    CGRect cursor = { .origin = { HSPACE, VSPACE }, .size = { font->width, font->height } };
    cursor.origin.x += font->width  * parent->emuState.cCol;
    cursor.origin.y += font->height * parent->emuState.cRow;

    CGContextSetRGBFillColor(ctx, 0.0, 1.0, 0.0, 0.7); // XXX: Cursor color shouldn't be constant
    CGContextFillRect(ctx, cursor);

    running = YES;
    redrawPending = NO;
    redrawCounter = 0;
}

- (void)resizeForTerminal
{
    int termWidth = parent->emuState.wCols * font->width + 2 * HSPACE;
    int termHeight = parent->emuState.wRows * font->height + 2 * VSPACE;
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
    int newRows = (frame.size.height - 2 * VSPACE) / font->height;
    int newCols = (frame.size.width  - 2 * HSPACE) / font->width;
    if(newRows != parent->emuState.wRows || newCols != parent->emuState.wCols) {
        [parent viewDidResize:self rows:newRows cols:newCols];
    }
    //if(newsize.ws_col != parent->emuState.wCols || newsize.ws_row != parent->emuState.wRows)
    //    [parent setWinSize:newsize];
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
