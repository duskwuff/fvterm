struct ConsoleKeyMap
{
    enum { 
        CKM_IGNORE,
        CKM_CURS,
        CKM_CSI,
        CKM_NUM,
    } type;
    int content;
} consoleKeyMappings[] = {
{   CKM_CURS,   'A' },  // 00 - up
{   CKM_CURS,   'B' },  // 01 - down
{   CKM_CURS,   'D' },  // 02 - left
{   CKM_CURS,   'C' },  // 03 - right
{   CKM_CSI,    'P' },  // 04 - F1 (PF1)
{   CKM_CSI,    'Q' },  // 05 - F2
{   CKM_CSI,    'R' },  // 06 - F3
{   CKM_CSI,    'S' },  // 07 - F4
{   CKM_NUM,    15 },   // 08 - F5 (funny story on this one)
{   CKM_NUM,    17 },   // 09 - F6
{   CKM_NUM,    18 },   // 0A - F7
{   CKM_NUM,    19 },   // 0B - F8
{   CKM_NUM,    20 },   // 0C - F9
{   CKM_NUM,    21 },   // 0D - F10
{   CKM_NUM,    23 },   // 0E - F11
{   CKM_NUM,    24 },   // 0F - F12
{   CKM_NUM,    25 },   // 10 - F13 (using VT220/rxvt codes here)
{   CKM_NUM,    26 },   // 11 - F14
{   CKM_NUM,    28 },   // 12 - F15
{   CKM_NUM,    29 },   // 13 - F16
{   CKM_NUM,    31 },   // 14 - F17
{   CKM_NUM,    32 },   // 15 - F18
{   CKM_NUM,    33 },   // 16 - F19
{   CKM_NUM,    34 },   // 17 - F20
{   CKM_NUM,    42 },   // 18 - F21 ("after F20 the codes are made up...")
{   CKM_NUM,    43 },   // 19 - F22
{   CKM_NUM,    44 },   // 1A - F23
{   CKM_NUM,    45 },   // 1B - F24
{   CKM_NUM,    46 },   // 1C - F25
{   CKM_NUM,    47 },   // 1D - F26
{   CKM_NUM,    48 },   // 1E - F27
{   CKM_NUM,    49 },   // 1F - F28
{   CKM_NUM,    50 },   // 20 - F29
{   CKM_NUM,    51 },   // 21 - F30
{   CKM_NUM,    52 },   // 22 - F31
{   CKM_NUM,    53 },   // 23 - F32
{   CKM_NUM,    54 },   // 24 - F33
{   CKM_NUM,    55 },   // 25 - F34
{   CKM_NUM,    56 },   // 26 - F35 (who the hell has 35 fkeys?)
{   CKM_NUM,    2 },    // 27 - insert
{   CKM_NUM,    3 },    // 28 - fwd del
{   CKM_NUM,    7 },    // 29 - home
{   CKM_NUM,    7 },    // 2A - begin (= home)
{   CKM_NUM,    8 },    // 2B - end
{   CKM_NUM,    5 },    // 2C - pgup
{   CKM_NUM,    6 },    // 2D - pgdn
{   CKM_IGNORE, 0 },    // 2E - print screen (here be silly keys)
{   CKM_IGNORE, 0 },    // 2F - scroll lock
{   CKM_IGNORE, 0 },    // 30 - pause
{   CKM_IGNORE, 0 },    // 31 - sysrq
{   CKM_IGNORE, 0 },    // 32 - break
{   CKM_IGNORE, 0 },    // 33 - reset
{   CKM_IGNORE, 0 },    // 34 - stop
{   CKM_NUM,    29 },   // 35 - menu
{   CKM_IGNORE, 0 },    // 36 - user
{   CKM_IGNORE, 0 },    // 37 - system
{   CKM_IGNORE, 0 },    // 38 - print
{   CKM_IGNORE, 0 },    // 39 - clear line
{   CKM_IGNORE, 0 },    // 3A - clear display
{   CKM_IGNORE, 0 },    // 3B - insert line
{   CKM_IGNORE, 0 },    // 3C - delete line
{   CKM_IGNORE, 0 },    // 3D - insert char
{   CKM_IGNORE, 0 },    // 3E - delete char
{   CKM_NUM,    5 },    // 3F - prev (= pgup)
{   CKM_NUM,    6 },    // 40 - next (= pgdn)
{   CKM_NUM,    4 },    // 41 - select
{   CKM_IGNORE, 0 },    // 42 - execute
{   CKM_IGNORE, 0 },    // 43 - undo
{   CKM_IGNORE, 0 },    // 44 - redo
{   CKM_NUM,    1 },    // 45 - find
{   CKM_NUM,    28 },   // 46 - help
{   CKM_IGNORE, 0 },    // 47 - mode switch
};