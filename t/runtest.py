import sys, os, re
from ctypes import *
fvterm = CDLL(os.path.join(
    os.path.dirname(__file__), "../build/libfvterm.dylib"))

class Fvterm(c_void_p): pass

fvterm.fvterm_init.restype = Fvterm
fvterm.fvterm_init.argtypes = [c_int, c_int]
fvterm.fvterm_free.restype = None
fvterm.fvterm_free.argtypes = [Fvterm]
fvterm.fvterm_write.restype = None
fvterm.fvterm_write.argtypes = [Fvterm, c_char_p, c_int]
fvterm.fvterm_getsize.restype = None
fvterm.fvterm_getsize.argtypes = [Fvterm, POINTER(c_int), POINTER(c_int)]
fvterm.fvterm_getcursor.restype = None
fvterm.fvterm_getcursor.argtypes = [Fvterm, POINTER(c_int), POINTER(c_int)]
fvterm.fvterm_getrowflags.restype = c_int
fvterm.fvterm_getrowflags.argtypes = [Fvterm, c_int]
fvterm.fvterm_getglyph.restype = c_int64
fvterm.fvterm_getglyph.argtypes = [Fvterm, c_int, c_int]

def unesc(txt):
    txt = re.sub(r"\\([0-9a-f]{2})", lambda x: chr(int(x.group(1), 16)), txt)
    txt = re.sub(r"\^(.)", lambda x: chr(ord(x.group(1)) & 0x1f), txt)
    return txt

class Check(object):
    def __init__(self, line, lineno):
        self.parse(line)
        self.lineno = lineno
    def test(self, term):
        return False

class TextCheck(Check):
    def parse(self, line):
        row, col, text = line.split(None, 3)[1:]
        self.row, self.col = int(row), int(col)
        self.text = unesc(text)
    def test(self, term):
        ok = True
        for i, ch in enumerate(self.text):
            glyph = fvterm.fvterm_getglyph(term, self.row, self.col + i)
            if ord(ch) != (glyph & 0xffff):
                print "Glyph mismatch at col %d: wanted %02x, got %02x" % (
                        self.col + i, ord(ch), glyph & 0xffff)
                ok = False
        return ok

class AttrCheck(Check):
    def parse(self, line):
        row, col, attr, mask = line.split(None, 2)[1:]
        self.row, self.col = int(row), int(col)
        self.attr = int(attr, 16)
        self.mask = int(mask, 16)

class CursorCheck(Check):
    def parse(self, line):
        row, col = line.split(None, 2)[1:]
        self.row, self.col = int(row), int(col)
    def test(self, term):
        row = c_int()
        col = c_int()
        fvterm.fvterm_getcursor(term, byref(row), byref(col))
        return self.row == row.value and self.col == col.value

checkmap = {
    "OUT": TextCheck,
    "ATTR": AttrCheck,
    "CURSOR": CursorCheck,
}

def runTest(testPath):
    testFile = file(testPath, "r")
    cols, rows = 80, 24
    inputs = []
    checks = []
    for lineno, line in enumerate(testFile):
        lineno += 1 # 1-base
        line = line.strip()
        if not line: continue
        word0 = line.split()[0]
        if word0 == "RES":
            cols, rows = line.split()[1:]
        elif word0 == "IN":
            inputs.append(unesc(line.split(None, 1)[1]))
        elif checkmap[word0]:
            checks.append(checkmap[word0](line, lineno))
        else:
            print "%s: unknown cmd %r at line %d" % (testPath, word0, lineno)

    term = fvterm.fvterm_init(24, 80)
    errors = 0
    for txt in inputs:
        fvterm.fvterm_write(term, txt, len(txt))
    for check in checks:
        if not check.test(term):
            print "%s: failed at line %d" % (testPath, check.lineno)
            errors += 1
    fvterm.fvterm_free(term)
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
