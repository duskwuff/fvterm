#import "Prefix.h"

#import "TerminalWindow.h"
#import "TerminalPTY.h"
#import "TerminalWindow_Emulation.h"
#import "TerminalView.h"

#ifndef unlikely
# define unlikely(x)     __builtin_expect((x),0)
#endif

#define DEFAULT(n, default) ((n) == 0 ? (default) : (n))
#define CAP_MIN(val, vmin) do { if((val) < (vmin)) val = vmin; } while(0)
#define CAP_MAX(val, vmax) do { if((val) > (vmax)) val = vmax; } while(0)
#define CAP_MIN_MAX(val, vmin, vmax) do { CAP_MIN(val, vmin); CAP_MAX(val, vmax); } while(0)

#define ATTR_PACK(ch, attr) (((uint64_t) (attr) << 32) | ((uint32_t) ch))

#define MAX_PARAMS 16

struct EmulationState {
    enum emuState {
        ST_GROUND,
        ST_ESC,
        //ST_ESC_INT - folded into ST_ESC
        ST_CSI,
        ST_CSI_INT,
        ST_CSI_PARM,
        ST_CSI_IGNORE,
        ST_OSC,
        ST_SOS,
        ST_DCS,
        ST_DCS_INT,
        ST_DCS_PARM,
        ST_DCS_IGNORE,
        ST_DCS_PASS,
    } state;
    uint32_t cursorAttr;

    BOOL wrapnext;
    int flags;
    int tscroll, bscroll;
    
    int saveRow, saveCol;
    uint32_t saveAttr;

    int params[MAX_PARAMS], paramPtr, paramVal;
    int priv, intermed;
    
    int utf8State;
    uint8_t utf8Buf[4];
};

#define MODE_WRAPAROUND     0x0001
#define MODE_REVWRAP        0x0002
#define MODE_ORIGIN         0x0004
#define MODE_LINEFEED       0x0008
#define MODE_INSERT         0x0010
#define MODE_SOFTSCROLL     0x0020
#define MODE_NEWLINE        0x0040
#define MODE_CURSORKEYS     0x0080
#define MODE_INVERT         0x0100

#define PACK_2(x,y)   ( ((x) << 8) | (y) )
#define PACK_3(x,y,z) ( ((x) << 16) | ((y) << 8) | (z))


@implementation TerminalWindow(Emulation)

static void row_fill(struct termRow *row, int start, int count, uint64_t value)
{
    memset_pattern8(&row->chars[start], &value, count * 8); // heavily optimized on x86 :D
    row->dirty = YES;
    row->wrapped = NO;
}

static void scroll_down(TerminalWindow *self, int top, int btm, int count)
{
    assert(count > 0);
    int clearStart;
    if(count > btm - top) // every row's getting cleared - don't bother rearranging the pointers at all
        clearStart = top;
    else {
        clearStart = btm - count + 1;
        struct termRow *keepBuf[count];
        memcpy(keepBuf, &self->rows[top], count * sizeof(struct termRow *));
        memmove(&self->rows[top], &self->rows[top + count], (clearStart - top) * sizeof(struct termRow *));
        memcpy(&self->rows[clearStart], keepBuf, count * sizeof(struct termRow *));
    }
    
    for(int i = clearStart; i <= btm; i++)
        row_fill(self->rows[i], 0, self->size.ws_col, ATTR_PACK(' ', self->S->cursorAttr));
}


static void scroll_up(TerminalWindow *self, int top, int btm, int count)
{
    // FIXME: unoptimized and inefficient
    assert(count > 0);
    while(count--) {
        struct termRow *movingRow = self->rows[btm];
        for(int i = btm; i > top; i--)
            self->rows[i] = self->rows[i - 1];
        self->rows[top] = movingRow;
        row_fill(movingRow, 0, self->size.ws_col, ATTR_PACK(' ', self->S->cursorAttr));
    }
}


static void term_index(TerminalWindow *self, int count)
{
    if(unlikely(count == 0)) return;
    struct EmulationState *S = self->S;
    
    if(count > 0) {
        // positive scroll = scroll down
        int dist = S->bscroll - self->cRow;
        if(dist >= count)
            self->cRow += count;
        else {
            self->cRow = S->bscroll;
            scroll_down(self, S->tscroll, S->bscroll, count - dist);
        }
    } else {
        count = -count; // negative scroll = scroll up, but reverse that now
        int dist = self->cRow - S->tscroll;
        if(dist >= count)
            self->cRow -= count;
        else {
            self->cRow = S->tscroll;
            scroll_up(self, S->tscroll, S->bscroll, count - dist);
        }
    }
}

static void term_cursorHorz(TerminalWindow *self, int count)
{
    struct EmulationState *S = self->S;
    
    // XXX: handle reverse wraparound
    self->cCol += count;
    CAP_MIN_MAX(self->cCol, 0, self->size.ws_col - 1);
    
    S->wrapnext = NO;
}


//////////////////////////////////////////////////////////////////////////////


- (void)_emulationInit
{
    S = malloc(sizeof(struct EmulationState));
    
    S->state = ST_GROUND;
    S->cursorAttr = ATTR_DEFAULT;
    S->wrapnext = NO;
    S->tscroll = 0;
    S->bscroll = size.ws_row - 1;
    
    S->flags = MODE_WRAPAROUND;
    
    S->utf8State = 0;
}


- (void)_emulationResize
{
    cRow = cCol = 0;
    
    S->tscroll = 0;
    S->bscroll = size.ws_row - 1;
    S->wrapnext = NO;
}


- (void)_emulationFree
{
    free(S);
}


//////////////////////////////////////////////////////////////////////////////


static void applyFlags(TerminalWindow *self, int flags)
{
    self->cursorKeyMode = !!(flags & MODE_CURSORKEYS);
    self->invertMode = !!(flags & MODE_INVERT); // XXX: need support for this in view...
}


static void act_clear(struct EmulationState *S)
{
    S->paramPtr = 0;
    S->paramVal = 0;
    S->priv = S->intermed = 0;
    bzero(S->params, sizeof(S->params));
}


//////////////////////////////////////////////////////////////////////////////


void do_controlChar(TerminalWindow *self, uint8_t ch) {
    struct EmulationState *S = self->S;
    switch(ch) {
        case 0x07: // BEL
            NSBeep(); // XXX: tweak!
            break;
            
        case 0x08: // BS
            term_cursorHorz(self, -1);
            break;
            
        case 0x09: // HT
            self->cCol = (self->cCol + 8) & ~7; // XXX: custom tab stops
            CAP_MAX(self->cCol, self->size.ws_col - 1); // XXX: how does this affect wrapping?
            S->wrapnext = NO;
            break;
            
        case 0x0A: // NL
        case 0x0B: // VT
        case 0x0C: // NP
            term_index(self, 1);
            if(S->flags & MODE_NEWLINE)
                self->cCol = 0;
            S->wrapnext = NO;
            break;
            
        case 0x0D: // CR
            self->cCol = 0;
            S->wrapnext = NO;
            break;
            
        case 0x1B: // ESC - handled further up the tree, cased here for sanity
            break;
            
        default:
            NSLog(@"Unhandled control char %x", ch);
    }
}
 

static void _output_char(TerminalWindow *self, struct EmulationState *S, uint32_t uch) {
    if(S->wrapnext) { // wrapping time
        if(S->flags & MODE_WRAPAROUND) {
            self->rows[self->cRow]->wrapped = YES;
            term_index(self, 1);
            self->cCol = 0;
        }
        S->wrapnext = NO;
    }
    
    self->rows[self->cRow]->chars[self->cCol++] = ATTR_PACK(uch, S->cursorAttr);
    self->rows[self->cRow]->dirty = YES;
    
    if(self->cCol == self->size.ws_col) { // hit the edge, set wrapnext
        self->cCol--;
        S->wrapnext = YES;
    }
}


static void do_ground(TerminalWindow *self, uint8_t ch) {
    struct EmulationState *S = self->S;
    
    // states:
    // 0: ground
    // 1: 2-byte seq
    // 2: 3-byte seq, 1st
    // 3: ^ 2nd

    switch(S->utf8State) {
        case 0:
            if(ch < 0xc2) {
                _output_char(self, S, ch);
            } else if(ch < 0xe0) {
                S->utf8Buf[0] = ch;
                S->utf8State = 1;
            } else if(ch < 0xf0) {
                S->utf8Buf[0] = ch;
                S->utf8State = 2;
            } else { // f0-ff
                _output_char(self, S, ch);
            }
            break;
            
        case 1:
            if(ch < 0x80 || ch >= 0xc0) {
                _output_char(self, S, S->utf8Buf[0]);
                _output_char(self, S, ch);
                S->utf8State = 0;
            } else {
                _output_char(self, S, ((S->utf8Buf[0] & 0x1f) << 6) | (ch & 0x3f));
                S->utf8State = 0;
            }
            break;
            
        case 2:
            if(ch < 0x80 || ch >= 0xc0) {
                _output_char(self, S, S->utf8Buf[0]);
                _output_char(self, S, ch);
                S->utf8State = 0;
            } else {
                S->utf8Buf[1] = ch;
                S->utf8State = 3;
            }
            break;
            
        case 3:
            if(ch < 0x80 || ch >= 0xc0) {
                _output_char(self, S, S->utf8Buf[0]);
                _output_char(self, S, S->utf8Buf[1]);
                _output_char(self, S, ch);
                S->utf8State = 0;
            } else {
                _output_char(self, S, ((S->utf8Buf[0] & 0x0f) << 12) | ((S->utf8Buf[1] & 0x3f) << 6) | (ch & 0x3f));
                S->utf8State = 0;
            }
            break;
    }
}


static enum emuState do_esc(TerminalWindow *self, uint8_t ch, enum emuState subState) {
    struct EmulationState *S = self->S;
    
    if(ch < 0x30) { // intermediate (we're guaranteed ch >= 0x20)
        if(S->intermed >= 0xFFFF)
            S->intermed = 0xFFFF;
        else
            S->intermed = (S->intermed << 8) | ch;
        return ST_ESC; // more! more!
        
    } else switch(S->intermed) {
        case 0:
            switch(ch) {
                case '7': // DECSC (Save Cursor)
                    S->saveRow = self->cRow;
                    S->saveCol = self->cCol;
                    S->saveAttr = S->cursorAttr;
                    break;
                    
                case '8': // DECRC (Restore Cursor)
                    self->cRow = S->saveRow;
                    self->cCol = S->saveCol;
                    S->cursorAttr = S->saveAttr;
                    // Now make sure we aren't doing something incredibly dumb
                    CAP_MAX(self->cRow, self->size.ws_row - 1);
                    CAP_MAX(self->cCol, self->size.ws_col - 1);
                    break;
                    
                case 'D': // IND
                    term_index(self, 1);
                    break;
                    
                case 'E': // NEL
                    term_index(self, 1);
                    self->cCol = 0;
                    break;
                    
                case 'M': // RI
                    term_index(self, -1);
                    break;
                    
                case '[': // CSI
                    act_clear(S);
                    return ST_CSI;
                    
                case ']': // OSC
                    act_clear(S);
                    return ST_OSC;
                    
                default:
                    NSLog(@"Unhandled ESC %c", ch);
            }
            break;
            
        case '#':
            switch(ch) {
                case '8': // DECALN
                    for(int i = 0; i < self->size.ws_row; i++)
                        row_fill(self->rows[i], 0, self->size.ws_col, ATTR_PACK('E', S->cursorAttr));
                    break;
                default:
                    NSLog(@"Unhandled ESC # %c", ch);
            }
            break;
            
        default:
            NSLog(@"Unhandled ESC intermediate combination %x -> %c", S->intermed, ch);
    }
            
    return ST_GROUND;
}


static void csi_dispatch(TerminalWindow *self, uint8_t final) {
    struct EmulationState *S = self->S;
    
    // XXX: need to deal with intermediate/private characters
    
    // shared variables
    int from, to, top, btm, row, val;
    
    switch(final) {
        case 'A': // CUU
            row = self->cRow;
            self->cRow -= DEFAULT(S->params[0], 1);
            if(row < S->tscroll)
                CAP_MIN(self->cRow, 0);
            else
                CAP_MIN(self->cRow, S->tscroll);
            S->wrapnext = NO;
            break;
            
        case 'B': // CUD
            row = self->cRow;
            self->cRow += DEFAULT(S->params[0], 1);
            if(row > S->bscroll)
                CAP_MAX(self->cRow, self->size.ws_row - 1);
            else
                CAP_MAX(self->cRow, S->bscroll);
            S->wrapnext = NO;
            break;
            
        case 'C': // CUF
            term_cursorHorz(self, DEFAULT(S->params[0], 1));
            break;
            
        case 'D': // CUB
            term_cursorHorz(self, -DEFAULT(S->params[0], 1));
            break;
            
            
        case 'H': // CUP
        case 'f': // HVP
            self->cRow = DEFAULT(S->params[0], 1) - 1;
            self->cCol = DEFAULT(S->params[1], 1) - 1;
            if(S->flags & MODE_ORIGIN) {
                self->cRow += S->tscroll;
                CAP_MAX(self->cRow, S->bscroll);
            } else
                CAP_MIN_MAX(self->cRow, 0, self->size.ws_row - 1);
            CAP_MIN_MAX(self->cCol, 0, self->size.ws_col - 1);
            S->wrapnext = NO;
            break;
            
            
        case 'J': // ED
            from = 0;
            to = self->size.ws_row - 1;
            switch(S->params[0]) {
                default:
                case 0:
                    from = self->cRow + 1;
                    break;
                case 1:
                    to = self->cRow - 1;
                    break;
                case 2:
                    break;
            }
            for(int i = from; i <= to; i++)
                row_fill(self->rows[i], 0, self->size.ws_col, ATTR_PACK(' ', S->cursorAttr));
            // FALL THROUGH (intentionally... ED erases partial lines too)
            
        case 'K': // EL
            from = 0;
            to = self->size.ws_col - 1;
            switch(S->params[0]) {
                default:
                case 0:
                    from = self->cCol;
                    break;
                case 1:
                    to = self->cCol;
                    break;
                case 2:
                    break;
            }
            row_fill(self->rows[self->cRow], from, to - from, ATTR_PACK(' ', S->cursorAttr));
            break;
            
        //case 'L': // IL  
        //case 'M': // DL
            
        case 'c': // "what are you"
            [self->pty writeData:[@"\e[?1;2c" dataUsingEncoding:NSUTF8StringEncoding]];
            break;

        case 'h':
        case 'l':
            val = (final == 'h');
#define APPLY_FLAG(mask) if(val) S->flags |= (mask); else S->flags &= ~(mask);
            for(int i = 0; i < S->paramPtr; i++)
                switch(S->priv) {
                    case 0:   // ANSI
                        switch(S->params[i]) {
                            //case 4:  // IRM (Insert/Replace)

                            case 20: // LNM (Linefeed Newline)
                                APPLY_FLAG(MODE_NEWLINE);
                                break;
                                
                            default:
                                NSLog(@"Unknown ANSI mode %d", S->params[i]);
                        }
                        break;
                        
                    case '?': // DEC private
                        switch(S->params[i]) {
                            case 1: // DECCKM (Cursor Key)
                                APPLY_FLAG(MODE_CURSORKEYS);
                                break;
                                
                            //case 2: // DECANM (VT52)
                                
                            case 3: // DECCOLM (Column)
                            {
                                struct winsize ws = self->size;
                                ws.ws_col = val ? 132 : 80;
                                [self setWinSize:ws];
                            }
                                break;
                                
                            case 4: // DECSCLM (Scrolling)
                                APPLY_FLAG(MODE_SOFTSCROLL);
                                break;
                                
                            case 5: // DECSCNM (Screen)
                                APPLY_FLAG(MODE_INVERT);
                                break;
                                
                            case 6: // DECOM (Origin)
                                APPLY_FLAG(MODE_ORIGIN);
                                break;
                                
                            case 7: // DECAWM (Auto-Wrap)
                                APPLY_FLAG(MODE_WRAPAROUND);
                                break;
                                
                            case 8: // DECARM (Auto-Repeat)
                            case 9: // DECINLM (Interlace)
                                break; // These features aren't applicable
                                
                            //case 40: // XTerm 132-column mode
                                
                            case 41: // XTerm curses hack
                                break; // hopefully this has been lost and forgotten by now
                                
                            default:
                                NSLog(@"Unknown DEC mode %d", S->params[i]);
                        }
                        break;
                        
                    default:
                        NSLog(@"Unknown super-private mode %c/%d", S->params[i]);
                }
            
            applyFlags(self, S->flags);
            
            break;
            
        case 'm': // SGR
            for(int i = 0; i < S->paramPtr; i++)
                switch(S->params[i]) {
                    case 0:
                        S->cursorAttr = ATTR_DEFAULT;
                        break;
                    case 1:
                        S->cursorAttr |= ATTR_BOLD;
                        break;
                    case 4:
                        S->cursorAttr |= ATTR_UNDERLINE;
                        break;
                    case 5:
                        S->cursorAttr |= ATTR_BLINK;
                        break;
                    case 7:
                        S->cursorAttr |= ATTR_REVERSE;
                        break;
                    //case 8: // XXX: invisible?
                    case 22:
                        S->cursorAttr &= ~ATTR_BOLD;
                        break;
                    case 24:
                        S->cursorAttr &= ~ATTR_UNDERLINE;
                        break;
                    case 25:
                        S->cursorAttr &= ~ATTR_BLINK;
                        break;
                    case 27:
                        S->cursorAttr &= ~ATTR_REVERSE;
                        break;
                    //case 28: // XXX: un-invisible
                    case 30 ... 37:
                        S->cursorAttr &= ~ATTR_FG7_MASK;
                        S->cursorAttr |= ATTR_CUSTFG | (S->params[i] - 30) << ATTR_FG_SHIFT;
                        break;
                    case 38: // extended foreground
                        if(++i >= S->paramPtr) break;   // next argument...
                        if(S->params[i] != 5) break;    // must be 5 to activate this (?!)
                        if(++i >= S->paramPtr) break;   // step ahead again
                        S->cursorAttr &= ~ATTR_FG_MASK;
                        S->cursorAttr |= ATTR_CUSTFG | (S->params[i] & 255) << ATTR_FG_SHIFT;
                        break;
                    case 39: // default FG
                        S->cursorAttr &= ~(ATTR_FG7_MASK | ATTR_CUSTFG);
                        S->cursorAttr |= 7 << ATTR_FG_SHIFT;
                        break;
                    case 40 ... 47:
                        S->cursorAttr &= ~ATTR_BG7_MASK;
                        S->cursorAttr |= ATTR_CUSTBG | (S->params[i] - 40) << ATTR_BG_SHIFT;
                        break;
                    case 48: // extended background
                        if(++i >= S->paramPtr) break;
                        if(S->params[i] != 5) break;
                        if(++i >= S->paramPtr) break;
                        S->cursorAttr &= ~ATTR_BG_MASK;
                        S->cursorAttr |= ATTR_CUSTBG | (S->params[i] & 255) << ATTR_BG_SHIFT;
                        break;
                    case 49: // default BG
                        S->cursorAttr &= ~(ATTR_BG7_MASK | ATTR_CUSTBG);
                        S->cursorAttr |= 0 << ATTR_BG_SHIFT;
                        break;
                    default:
                        NSLog(@"Unknown SGR %d", S->params[i]);
                }
            break;
            
        case 'r': // DECSTBM
            top = DEFAULT(S->params[0], 1);
            btm = DEFAULT(S->params[1], 65535);
            CAP_MAX(btm, self->size.ws_row);
            if(btm > top) {
                S->tscroll = top - 1;
                S->bscroll = btm - 1;
                self->cRow = (S->flags & MODE_ORIGIN) ? S->tscroll : 0;
                self->cCol = 0;
            }
            break;

        default:
        {
            NSMutableArray *argArray = [NSMutableArray arrayWithCapacity:MAX_PARAMS];
            for(int i = 0; i < S->paramPtr; i++)
                [argArray addObject:[NSString stringWithFormat:@"%d", S->params[i]]];
            NSLog(@"Unhandled CSI %@%c (priv %x, intermed %x)",
                  [argArray componentsJoinedByString:@";"], final, S->priv, S->intermed);
            break;
        }
    }
}


static enum emuState do_csi(TerminalWindow *self, uint8_t ch, enum emuState subState) {
    struct EmulationState *S = self->S;

    // Ignore everything when in "ignore" state
    if(unlikely(subState == ST_CSI_IGNORE))
        return subState;
    
    if(ch < 0x30) { // intermediate char (guaranteed to be >= 0x20)
        if(S->intermed >= 0xFFFF)
            S->intermed = 0xFFFF;
        else
            S->intermed = (S->intermed << 8) | ch;
        return ST_CSI_INT;
        
    } else if(ch < 0x3A) { // digit
        if(subState == ST_CSI_INT)
            return ST_CSI_IGNORE; // intermediate -> param isn't a valid sequence
        CAP_MIN(S->paramVal, 0);
        S->paramVal = 10 * S->paramVal + (ch - 0x30);
        CAP_MAX(S->paramVal, 16383);
        return ST_CSI_PARM;
        
    } else if(ch == 0x3A) {
        return ST_CSI_IGNORE; // always invalid
        
    } else if(ch == 0x3B) {
        if(subState == ST_CSI_INT)
            return ST_CSI_IGNORE; // intermediate -> param sep isn't a valid sequence
        if(S->paramPtr < MAX_PARAMS)
            S->params[S->paramPtr++] = S->paramVal;
        S->paramVal = 0;
        return ST_CSI_PARM;
        
    } else if(ch < 0x40) { // private markers
        if(subState != ST_CSI) // only allowed as first in a param sequence
            return ST_CSI_IGNORE;
        S->priv = ch;
        return ST_CSI_PARM;
        
    } else { // 0x40+: dispatch!
        // Accumulate the last parameter
        if(S->paramPtr < MAX_PARAMS)
            S->params[S->paramPtr++] = S->paramVal;
        
        if(subState != ST_CSI_IGNORE)
            csi_dispatch(self, ch);
        return ST_GROUND;
    }        
}


- (void)ptyInput:(TerminalPTY *)pty data:(NSData *)data
{
    const unsigned char *buf = [data bytes];
    size_t len = [data length];
    enum emuState state = S->state;
    
    for(int i = 0; i < len; i++) {
        uint8_t ch = buf[i];

        // "Anywhere" actions
        switch(ch) {
            case 0x00:
                continue;
                
            case 0x1B:
                state = ST_ESC;
                act_clear(S);
                continue;
                
/* THESE ARE INCOMPATIBLE WITH UTF-8 OUTPUT
            case 0x9B:
                state = ST_CSI;
                act_clear(S);
                break;
                
            case 0x9C:
                state = ST_GROUND;
                continue;
                
            case 0x9D:
                state = ST_OSC;
                act_clear(S);
                break;
*/
        }
        
        // State-based actions
        switch(state) {
            case ST_GROUND:
                if(ch < 0x20)
                    do_controlChar(self, ch);
                else
                    do_ground(self, ch);
                break;

            case ST_ESC:
                if(ch < 0x20)
                    do_controlChar(self, ch);
                else
                    state = do_esc(self, ch, state);
                break;

            case ST_CSI:
            case ST_CSI_INT:
            case ST_CSI_PARM:
            case ST_CSI_IGNORE:
                if(ch < 0x20)
                    do_controlChar(self, ch);
                else
                    state = do_csi(self, ch, state);
                break;

            case ST_OSC:
#warning FIXME
                // ...
                break;

            default:
                NSLog(@"Got into unhandled state");
                abort();
        }
    }
    
    S->state = state;
    
    if(S->flags & MODE_SOFTSCROLL)
        [view setNeedsDisplay:YES];
    else
        [view triggerRedrawWithDataLength:len];
}

@end
