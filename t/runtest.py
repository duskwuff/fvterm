import sys, os, re
from ctypes import *
fvterm = CDLL(os.path.join(os.path.dirname(__file__),
    "../build/libfvterm.dylib"))

class Fvterm(c_void_p):
    @classmethod
    def init(cls, rows, cols):
        return fvterm.fvterm_init(rows, cols)
    def free(self):
        fvterm.fvterm_free(self)
    def write(self, bytes, len):
        fvterm.fvterm_write(self, bytes, len)
    def setsize(self, r, c):
        fvterm.fvterm_setsize(self, r, c)
    def getsize(self):
        r = c_int()
        c = c_int()
        fvterm.fvterm_getsize(self, byref(r), byref(c))
        return (r.value, c.value)
    def getcursor(self):
        r = c_int()
        c = c_int()
        fvterm.fvterm_getcursor(self, byref(r), byref(c))
        return (r.value, c.value)
    def getrowflags(self, r):
        return fvterm.fvterm_getrowflags(r)
    def getglyph(self, r, c):
        return fvterm.fvterm_getglyph(self, r, c)

fvterm.fvterm_init.restype = Fvterm
fvterm.fvterm_init.argtypes = [c_int, c_int]
fvterm.fvterm_free.restype = None
fvterm.fvterm_free.argtypes = [Fvterm]
fvterm.fvterm_write.restype = None
fvterm.fvterm_write.argtypes = [Fvterm, c_char_p, c_int]
fvterm.fvterm_setsize.restype = None
fvterm.fvterm_setsize.argtypes = [Fvterm, c_int, c_int]
fvterm.fvterm_getsize.restype = None
fvterm.fvterm_getsize.argtypes = [Fvterm, POINTER(c_int), POINTER(c_int)]
fvterm.fvterm_getcursor.restype = None
fvterm.fvterm_getcursor.argtypes = [Fvterm, POINTER(c_int), POINTER(c_int)]
fvterm.fvterm_getrowflags.restype = c_int
fvterm.fvterm_getrowflags.argtypes = [Fvterm, c_int]
fvterm.fvterm_getglyph.restype = c_int64
fvterm.fvterm_getglyph.argtypes = [Fvterm, c_int, c_int]

def unesc(txt):
    txt = re.sub(r"\^(.)", lambda x: chr(ord(x.group(1)) & 0x1f), txt)
    txt = re.sub(r"\\([0-9a-fA-F]{2})", lambda x: chr(int(x.group(1), 16)), txt)
    return txt


##############################################################################
# Actions

class TestAction(object):
    def __init__(self, line, lineno):
        self.parse(line)
        self.lineno = lineno
    def run(self, term, flags):
        raise NotImplementedError

class WriteInputAction(TestAction):
    def parse(self, line):
        self.text = unesc(line.split(None, 1)[1])
    def run(self, term, flags):
        if flags.get("crnl", False):
            text = self.text.replace("\n", "\r\n")
        else:
            text = self.text
        term.write(text, len(text))

class SetResolutionAction(TestAction):
    def parse(self, line):
        cols, rows = line.split(None, 3)[1:]
        self.cols, self.rows = int(cols), int(rows)
    def run(self, term, flags):
        term.setsize(self.rows, self.cols)

class DumpTerminalAction(TestAction):
    def parse(self, line):
        self.mode = line.split(None, 1)[1:]
    def run(self, term, flags):
        rows, cols = term.getsize()
        for r in range(rows):
            line = "%3d: " % r
            for c in range(cols):
                glyph = term.getglyph(r, c)
                gchar = glyph & 0xffff
                if gchar >= 0x20 and gchar < 0x7f:
                    line += chr(gchar)
                else:
                    line += "\e[1m?\e[m",
            print line

class CrNlTranslationAction(TestAction):
    def parse(self, line):
        self.flag = bool(int(line.split(None, 1)[1]))
    def run(self, term, flags):
        flags["crnl"] = self.flag

##############################################################################
# Checks

class CheckFailed(Exception): pass

class TextCheck(TestAction):
    def parse(self, line):
        row, col, text = line.split(None, 3)[1:]
        self.row, self.col = int(row), int(col)
        self.text = unesc(text)
    def run(self, term, flags):
        for i, ch in enumerate(self.text):
            glyph = term.getglyph(self.row, self.col + i)
            if ord(ch) != (glyph & 0xffff):
                raise CheckFailed("Wrong glyph @ col %d: wanted %02x, got %02x"
                        % (self.col + i, ord(ch), glyph & 0xffff))

class AttrCheck(TestAction):
    def parse(self, line):
        row, col, attr, mask = line.split(None, 2)[1:]
        self.row, self.col = int(row), int(col)
        self.attr = int(attr, 16)
        self.mask = int(mask, 16)

class CursorCheck(TestAction):
    def parse(self, line):
        row, col = line.split(None, 2)[1:]
        self.row, self.col = int(row), int(col)
    def run(self, term, flags):
        row, col = term.getcursor()
        if row != self.row or col != self.col:
            raise CheckFailed("Cursor in wrong place: row %d, col %d" %
                    (row, col))


##############################################################################
# Meta

actmap = {
    "IN": WriteInputAction,
    "RES": SetResolutionAction,
    "OUT": TextCheck,
    "ATTR": AttrCheck,
    "CURSOR": CursorCheck,
    "DUMP": DumpTerminalAction,
    "CRNL": CrNlTranslationAction,
}


##############################################################################
# Test runner

def runTest(testPath):
    testFile = file(testPath, "r")
    cols, rows = 80, 24
    inputs = []
    actions = []
    for lineno, line in enumerate(testFile):
        lineno += 1 # 1-base
        line = line.strip()
        if not line or line.startswith("#"): continue
        word0 = line.split()[0]
        if word0 in actmap:
            actions.append(actmap[word0](line, lineno))
        else:
            print "%s: unknown cmd %r at line %d" % (testPath, word0, lineno)
            return 1

    errors = 0
    if not actions:
        print "%s: didn't define any actions!" % testPath
        return 1

    if isinstance(actions[0], SetResolutionAction):
        sra = actions.pop(0)
        cols, rows = sra.cols, sra.rows

    term = Fvterm.init(rows, cols)
    flags = {}

    for act in actions:
        try:
            act.run(term, flags)
        except CheckFailed, cfe:
            print "%s:%d: %s" % (testPath, act.lineno, cfe)
            errors += 1

    term.free()
    return errors

errors = 0
tests = 0
for arg in sys.argv[1:]:
    errors += runTest(arg)
    tests += 1
if errors == 0:
    print "%d tests passed" % tests
    sys.exit(0)
else:
    print "%d error(s)" % errors
    sys.exit(1)

