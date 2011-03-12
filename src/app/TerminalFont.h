#define FVFONT_CHARS_WIDE 32
#define FVFONT_CHARS_HIGH 8
#define FVFONT_NPAGES 512

struct fvfont_header {
    uint32_t signature;
    uint16_t width, height;
    uint8_t reserved[8];
    uint32_t page_offsets[FVFONT_NPAGES];
} __attribute__((packed));

@interface TerminalFont : NSObject {
    NSString *pageFiles[FVFONT_NPAGES];
@public
    int width, height;
    void *unpackedPages[FVFONT_NPAGES];
}

+ (id)loadFont:(NSString *)name;
- (BOOL)unpackPage:(int)page;

@end
