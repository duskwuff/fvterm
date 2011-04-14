#define FVFONT_CHARS_WIDE 32
#define FVFONT_CHARS_HIGH 8
#define FVFONT_NPAGES 512

@interface TerminalFont : NSObject {
    NSString *pageFiles[FVFONT_NPAGES];
@public
    int width, height, baseline, midline;
    void *unpackedPages[FVFONT_NPAGES];
    BOOL brightbold;
}

+ (id)loadFont:(NSString *)name;
- (id)initWithConfig:(NSDictionary *)dict;
- (BOOL)unpackPage:(int)page;

@end
