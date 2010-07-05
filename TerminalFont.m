#import "Prefix.h"

#import "TerminalFont.h"

@implementation TerminalFont

- (id)initWithFile:(NSString *)path
{
    if(!(self = [super init])) {
        return nil;
    }
    
    writeMode = NO;
    
    if(!(mapping = [[NSData dataWithContentsOfMappedFile:path] retain])) {
        NSLog(@"TerminalFont: dataWithContentsOfMappedFile failed on %@", path);
        [self release];
        return nil;
    }
    
    struct fvfont_header *hdr = (struct fvfont_header *) [mapping bytes];
    
    if(ntohl(hdr->signature) != FVFONT_SIGNATURE) {
        NSLog(@"Bad signature on font at %@", path);
        [self release];
        return nil;
    }
    
    width  = ntohs(hdr->width);
    height = ntohs(hdr->height);
    
    for(int i = 0; i < FVFONT_NPAGES; i++) {
        off_t offset = ntohl(hdr->page_offsets[i]);
        packedPages[i] = offset ? (((void *) hdr) + offset) : NULL;
        unpackedPages[i] = NULL;
    }
    
    return self;
}

- (id)initWithWidth:(int)in_width height:(int)in_height
{
    if(!(self = [super init])) {
        return nil;
    }
    
    width = in_width;
    height = in_height;
    writeMode = YES;
    
    return self;
}

- (BOOL)loadPage:(int)page fromImage:(NSImage *)img
{
    if(!writeMode) {
        NSLog(@"Can't load page into a non-writable font!");
        abort();
    }
    
    NSBitmapImageRep *rep = nil;
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
        NSLog(@"Couldn't find a representation of the right size for this font!\n");
        return NO;
    }
    
    uint8_t *packbuf = calloc(height * width * FVFONT_CHARS_HIGH * FVFONT_CHARS_WIDE, sizeof(uint8_t));
    uint8_t *packptr = packbuf;
    
    for(int y = 0; y < height * FVFONT_CHARS_HIGH; y++) {
        for(int x = 0; x < width * FVFONT_CHARS_WIDE; x++) {
            NSColor *c = [rep colorAtX:x y:y];
            *packptr++ = ([c whiteComponent] > 0.5) ? 0x00 : 0xff;
        }
    }
    
    packedPages[page] = packbuf;
    return YES;
}

- (BOOL)unpackPage:(int)page
{
    if(page < 0 || page >= FVFONT_NPAGES) {
        return NO;
    }
    
    if(packedPages[page] == NULL) {
        return NO;
    }

    uint8_t *p = packedPages[page];
    uint8_t *g = unpackedPages[page] = malloc(width * height * FVFONT_CHARS_WIDE * FVFONT_CHARS_HIGH);

#warning leakety leak leak

    int bit = 0;
    for(int y = 0; y < height * FVFONT_CHARS_HIGH; y++) {
        for(int x = 0; x < width * FVFONT_CHARS_WIDE; x++) {
            *g++ = (*p & (1 << bit++)) ? 0xff : 0x00;
            if(bit == 8) {
                bit = 0;
                p++;
            }
        }
    }
    
    return YES;
}

- (BOOL)writeToFile:(NSString *)path
{
    struct fvfont_header hdr;
    NSFileHandle *fh = [NSFileHandle fileHandleForWritingAtPath:path];
    if(!fh) return NO;
    
    size_t pageSize = width * height * FVFONT_CHARS_HIGH * FVFONT_CHARS_WIDE;
    pageSize = (pageSize / 8) + ((pageSize % 8) ? 1 : 0);
    hdr.signature = htonl(FVFONT_SIGNATURE);
    hdr.width     = htons(width);
    hdr.height    = htons(height);
    
    size_t offset = sizeof(hdr);
    for(int i = 0; i < FVFONT_NPAGES; i++) {
        if(packedPages[i]) {
            hdr.page_offsets[i] = htonl(offset);
            offset += pageSize;
        } else {
            hdr.page_offsets[i] = htonl(0);
        }
    }
    
    [fh writeData:[[NSData alloc] initWithBytesNoCopy:&hdr length:sizeof(hdr) freeWhenDone:NO]];
    
    uint8_t *pageBuf = malloc(pageSize);
    
    for(int i = 0; i < FVFONT_NPAGES; i++) {
        if(packedPages[i]) {
            uint8_t *pagePtr = pageBuf, *srcPtr = packedPages[i];
            NSLog(@"Writing packed page %d", i);
            
            int bit = 0, accum = 0;
            for(int y = 0; y < height * FVFONT_CHARS_HIGH; y++) {
                for(int x = 0; x < width * FVFONT_CHARS_WIDE; x++) {
                    if(*srcPtr++) accum |= (1 << bit);
                    if(++bit == 8) {
                        *pagePtr++ = accum;
                        bit = 0;
                        accum = 0;
                    }
                }
            }
            if(bit) {
                *pagePtr++ = accum;
            }
            
            [fh writeData:[[NSData alloc] initWithBytesNoCopy:pageBuf length:pageSize freeWhenDone:NO]];
        }
    }
    
    free(pageBuf);
    
    [fh closeFile];
    
    return YES;
}

@end
