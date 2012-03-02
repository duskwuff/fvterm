#import <Cocoa/Cocoa.h>

#import "TerminalFont.h"

static NSDictionary *fontPlist = NULL;
static NSMutableDictionary *loadedFonts = NULL;

@implementation TerminalFont

+ (void)_loadFontPlist
{
    if(!fontPlist) {
        fontPlist = [[NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"fonts" ofType:@"plist"]] retain];
        NSAssert(fontPlist != NULL, @"couldn't load fonts.plist");
    }
}

+ (NSArray *)availableFonts
{
    [self _loadFontPlist];
    return [fontPlist allKeys];
}

+ (id)loadFont:(NSString *)name
{
    [self _loadFontPlist];

    if(loadedFonts) {
        id font = [loadedFonts objectForKey:name];
        if(font) return font;
    } else {
        loadedFonts = [[NSMutableDictionary dictionary] retain];
    }

    NSDictionary *fontInfo = [fontPlist objectForKey:name];
    if(!fontInfo) return nil;

    id font = [[TerminalFont alloc] initWithConfig:fontInfo];
    [loadedFonts setObject:font forKey:name];

    return [font autorelease];
}

- (id)initWithConfig:(NSDictionary *)dict
{
    if(!(self = [super init])) return nil;

    width = [[dict objectForKey:@"width"] intValue];
    height = [[dict objectForKey:@"height"] intValue];
    baseline = [[dict objectForKey:@"baseline"] intValue];
    midline = [[dict objectForKey:@"midline"] intValue];
    brightbold = [[dict objectForKey:@"brightbold"] boolValue];

    for(int i = 0; i < FVFONT_NPAGES; i++)
        pageFiles[i] = NULL;

    NSDictionary *pages = [dict objectForKey:@"pages"];

    for(NSString *k in pages) {
        int pageno = -1;
        sscanf([k UTF8String], "%x", &pageno);
        if(pageno < 0 || pageno > FVFONT_NPAGES)
            continue;
        NSString *filename = [[NSBundle mainBundle] pathForImageResource:[pages objectForKey:k]];
        if(filename)
            pageFiles[pageno] = [filename retain];
    }

    return self;
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

    NSBitmapImageRep *rep = NULL;
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
