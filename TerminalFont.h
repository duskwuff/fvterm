#define FVFONT_CHARS_WIDE 32
#define FVFONT_CHARS_HIGH 8
#define FVFONT_NPAGES 512
#define FVFONT_SIGNATURE 0x6676ff01

struct fvfont_header {
    uint32_t signature;
    uint16_t width, height;
    uint8_t reserved[8];
    uint32_t page_offsets[FVFONT_NPAGES];
} __attribute__((packed));

@interface TerminalFont : NSObject {
    BOOL writeMode;
    NSData *mapping;
    void *packedPages[FVFONT_NPAGES];
@public
    int width, height;
    void *unpackedPages[FVFONT_NPAGES];
}

- (id)initWithFile:(NSString *)path;
- (id)initWithWidth:(int)width height:(int)height;
- (BOOL)loadPage:(int)page fromImage:(NSImage *)img;
- (BOOL)unpackPage:(int)page;
- (BOOL)writeToFile:(NSString *)path;

@end
