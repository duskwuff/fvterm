#import <stdio.h>

#import "TerminalFont.h"

int main(int argc, char **argv)
{
    NSAutoreleasePool *arp = [[NSAutoreleasePool alloc] init];

    if(argc != 3) {
        fprintf(stderr, "Usage: fontpacker fontconfig.ini output.vtf\n");
        exit(1);
    }
    
    FILE *fontconfig = fopen(argv[1], "r");
    if(!fontconfig) {
        perror(argv[1]);
        exit(1);
    }
    
    NSString *fontdir = [[NSString stringWithUTF8String:argv[1]] stringByDeletingLastPathComponent];
    
    int width = -1, height = -1;
    NSImage *pagefiles[FVFONT_NPAGES];
    
    char linebuf[512];
    int linectr = 0;
    while(fgets(linebuf, sizeof(linebuf), fontconfig) != NULL) {
        linectr += 1;
        char *line = linebuf;
        char *command = strsep(&line, " \t\n");
        if(command[0] == '\0') continue;
        if(!strcmp(command, "dim")) {
            if(sscanf(line, "%d %d", &width, &height) != 2) {
                fprintf(stderr, "Bad dim command at line %d\n", linectr);
                exit(1);
            }
        } else if(!strcmp(command, "page")) {
            int pageno;
            char pagefile[32];
            if(sscanf(line, "%x %30s", &pageno, pagefile) != 2) {
                fprintf(stderr, "Bad page command at line %d\n", linectr);
                exit(1);
            }
            
            if(pageno < 0 || pageno > FVFONT_NPAGES) {
                fprintf(stderr, "Invalid page number at line %d\n", linectr);
                exit(1);
            }
            
            printf("page %d = %s\n", pageno, pagefile);
            NSImage *img = pagefiles[pageno] = [[NSImage alloc] initWithContentsOfFile:[fontdir stringByAppendingPathComponent:[NSString stringWithUTF8String:pagefile]]];
            if(!img) {
                fprintf(stderr, "Couldn't load image %s\n", pagefile);
                exit(1);
            }
        }
    }
    
    if(width < 0 || height < 0) {
        fprintf(stderr, "Font info string never defined any dimensions...\n");
        exit(1);
    }
    
    TerminalFont *font = [[TerminalFont alloc] initWithWidth:width height:height];
    
    for(int i = 0; i < FVFONT_NPAGES; i++) {
        if(!pagefiles[i]) continue;
        if(![font loadPage:i fromImage:pagefiles[i]]) {
            fprintf(stderr, "Couldn't load page %03x\n", i);
            exit(1);
        }
    }
    
    if(![font writeToFile:[NSString stringWithUTF8String:argv[2]]]) {
        fprintf(stderr, "Failed writing out font to %s\n", argv[2]);
        exit(1);
    }
    
    fclose(fontconfig);
 
    [arp release];
    exit(0);
}