#import "TerminalFont.h"

static NSDictionary *fontPlist = NULL;

@implementation TerminalFont

+ (id) loadFont:(NSString *)name
{
    if(!fontPlist) {
        fontPlist = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fonts" ofType:@"plist"]];
        NSAssert(fontPlist != NULL, @"couldn't load fonts.plist");
    }
    
    NSDictionary *fontInfo = [fontPlist objectForKey:name];
    if(!fontInfo) return nil;
    
    TerminalFont *font = [[TerminalFont alloc] init];
    font->width =  [(NSNumber *)[fontInfo objectForKey:@"width"] intValue];
    font->height = [(NSNumber *)[fontInfo objectForKey:@"height"] intValue];
    for(int i = 0; i < FVFONT_NPAGES; i++) {
        font->pageFiles[i] = NULL;
    }
    NSDictionary *pages = [fontInfo objectForKey:@"pages"];
    for(NSString *k in pages) {
        int pageno = -1;
        sscanf([k UTF8String], "%x", &pageno);
        if(pageno < 0 || pageno > FVFONT_NPAGES) continue;
        NSString *filename = [[NSBundle mainBundle] pathForImageResource:[pages objectForKey:k]];
        if(filename) font->pageFiles[pageno] = [filename retain];
    }
    return font;
}

- (void) dealloc
{
    for(int i = 0; i < FVFONT_NPAGES; i++) {
        if(pageFiles[i])
            [pageFiles[i] release];
        if(unpackedPages[i])
            free(unpackedPages[i]);
    }
    [super dealloc];
}

- (id)initWithWidth:(int)in_width height:(int)in_height
{
    if(!(self = [super init])) {
        return nil;
    }

    width = in_width;
    height = in_height;

    return self;
}

- (BOOL)loadPage:(int)page fromImage:(NSImage *)img
{
    NSBitmapImageRep *rep = nil;

    if(!rep) {
        NSLog(@"Couldn't find a representation of the right size for this font!\n");
        return NO;
    }

    uint8_t *packbuf = calloc(height * width * FVFONT_CHARS_HIGH * FVFONT_CHARS_WIDE, sizeof(uint8_t));
    uint8_t *packptr = packbuf;

    for(int y = 0; y < height * FVFONT_CHARS_HIGH; y++) {
        for(int x = 0; x < width * FVFONT_CHARS_WIDE; x++) {
            NSColor *c = [rep colorAtX:x y:y];
            CGFloat components[8];
            [c getComponents:components];
            *packptr++ = (components[0] > 0.5) ? 0x00 : 0xff;
        }
    }

    return YES;
}

- (BOOL)unpackPage:(int)page
{
    if(page < 0 || page >= FVFONT_NPAGES)
        return NO;

    if(unpackedPages[page] != NULL)
        return NO;

    if(pageFiles[page] == NULL)
        return NO;

    NSImage *img = [[NSImage alloc] initWithContentsOfFile:pageFiles[page]];
    if(!img) {
        NSLog(@"couldn't load image %@", pageFiles[page]);
        return NO;
    }

    NSBitmapImageRep *rep;
    for(NSBitmapImageRep *r in [img representations]) {
        if([r class] != [NSBitmapImageRep class]) {
            NSLog(@"Found unexpected %@ representation...", r);
            continue;
        }
        if([r pixelsWide] == (width * FVFONT_CHARS_WIDE) && [r pixelsHigh] == (height * FVFONT_CHARS_HIGH)) {
            rep = r;
        }
    }
    
    if(!rep) {
        NSLog(@"couldn't find any appropriately sized/typed reps in %@", img);
        [img release];
        return NO;
    }
    
    uint8_t *packptr = unpackedPages[page] = malloc(height * width * FVFONT_CHARS_WIDE * FVFONT_CHARS_HIGH);

    for(int y = 0; y < height * FVFONT_CHARS_HIGH; y++) {
        for(int x = 0; x < width * FVFONT_CHARS_WIDE; x++) {
            CGFloat components[8];
            [[rep colorAtX:x y:y] getComponents:components];
            *packptr++ = (components[0] > 0.5) ? 0x00 : 0xff;
        }
    }
    
    [img release];

    return YES;
}

@end
