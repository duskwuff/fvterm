#define ATTR_FG_MASK    0x000000ffUL
#define ATTR_FG7_MASK   0x00000007UL
#define ATTR_FG_SHIFT   0
#define ATTR_BG_MASK    0x0000ff00UL
#define ATTR_BG7_MASK   0x00000700UL
#define ATTR_BG_SHIFT   8
#define ATTR_MD_MASK    0xffff0000UL
#define ATTR_MD_SHIFT   16

#define ATTR_DEFAULT    0x00000007UL

#define ATTR_BOLD       0x00010008UL
#define ATTR_UNDERLINE  0x00020000UL
#define ATTR_BLINK      0x00040000UL
#define ATTR_REVERSE    0x00080000UL
#define ATTR_CUSTFG     0x00100000UL
#define ATTR_CUSTBG     0x00200000UL

#define PAL_DEFAULT_FG 256
#define PAL_DEFAULT_BG 257

@interface TerminalWindow(Emulation)
- (void)_emulationInit;
- (void)_emulationResize;
- (void)_emulationFree;
- (void)ptyInput:(TerminalPTY *)pty data:(NSData *)data;
@end