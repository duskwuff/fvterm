import sys, os, re
from ctypes import *

##############################################################################

class Fvterm(c_void_p):
    @classmethod
    def init(cls, rows, cols):
        return Fvterm.lib.fvterm_init(rows, cols)
    def free(self):
        Fvterm.lib.fvterm_free(self)
    def write(self, bytes, len):
        Fvterm.lib.fvterm_write(self, bytes, len)
    def setsize(self, r, c):
        Fvterm.lib.fvterm_setsize(self, r, c)
    def getsize(self):
        r = c_int()
        c = c_int()
        Fvterm.lib.fvterm_getsize(self, byref(r), byref(c))
        return (r.value, c.value)
    def getcursor(self):
        r = c_int()
        c = c_int()
        Fvterm.lib.fvterm_getcursor(self, byref(r), byref(c))
        return (r.value, c.value)
    def getrowflags(self, r):
        return Fvterm.lib.fvterm_getrowflags(r)
    def getglyph(self, r, c):
        return Fvterm.lib.fvterm_getglyph(self, r, c)

    @classmethod
    def loadlib(cls, path):
        Fvterm.lib = fvterm = CDLL(path)
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

##############################################################################

class CheckFailed(Exception): pass

class TestRunner(object):
    def __init__(self, line, flags={}):
        self.text = line.strip()
        self.flags = flags

    def _trans(self, x):
        if "seq" in self.flags:
            x = x.replace("\\#", str(self.flags["seq"]))
        x = x.replace("\\n", "\n")
        x = x.replace("\\r", "\r")
        x = x.replace("\\t", "\t")
        x = x.replace("\\s", " ")
        x = re.sub(r"\^(.)",
                lambda x: chr(ord(x.group(1)) & 0x1f), x)
        x = re.sub(r"\\([0-9a-fA-F]{2})",
                lambda x: chr(int(x.group(1), 16)), x)
        return x

    def getWordRaw(self):
        try:
            w, self.text = self.text.split(None, 1)
            return w
        except:
            w, self.text = self.text, ""
            return w

    def getWord(self):
        return self._trans(self.getWordRaw())

    def getInt(self):
        return int(self.getWord())

    def getLine(self):
        return self._trans(self.text)

    def doCommand(self, term):
        if self.text == "" or self.text.startswith("#"): return
        cmd = self.getWordRaw()
        cmdfunc = getattr(self, "do_" + cmd, self.do_badcmd)
        cmdfunc(term)

    def do_badcmd(self, term):
        raise CheckFailed("Unknown directive")


    def do_IN(self, term):
        text = self.getLine()
        term.write(text, len(text))

    def do_RES(self, term):
        cols, rows = self.getInt(), self.getInt()
        term.setsize(rows, cols)

    def do_REP(self, term):
        count = self.getInt()
        for i in range(count):
            self.__class__(self.text, self.flags).doCommand(term)

    def do_SEQ(self, term):
        lo, hi = self.getInt(), self.getInt()
        lflags = dict(self.flags)
        for i in range(lo, hi + 1):
            lflags["seq"] = i
            self.__class__(self.text, lflags).doCommand(term)


    def do_DUMP(self, term):
        mode = self.getWord()
        rows, cols = term.getsize()
        print "Dump:"
        cr, cc = term.getcursor()
        for r in range(rows):
            text = "%3d: |" % r
            for c in range(cols):
                glyph = term.getglyph(r, c)
                gchar = glyph & 0xffff
                if r == cr and c == cc:
                    text += "\033[7m"
                if gchar >= 0x20 and gchar < 0x7f:
                    text += chr(gchar)
                else:
                    text += "\033[1m?"
                text += "\033[m"
            text += "|"
            print text

    def do_OUT(self, term):
        row, col = self.getInt(), self.getInt()
        text = self.getLine()
        for i, ch in enumerate(text):
            glyph = term.getglyph(row, col + i)
            if ord(ch) != (glyph & 0xffff):
                raise CheckFailed("Wrong glyph @ col %d: wanted %02x, got %02x" % (
                    col + i, ord(ch), glyph & 0xffff))

    def do_CURSOR(self, term):
        xrow, xcol = self.getInt(), self.getInt()
        crow, ccol = term.getcursor()
        if crow != xrow or ccol != xcol:
            raise CheckFailed("Wrong cursor: wanted %d/%d, got %d/%d" % (
                xrow, xcol, crow, ccol))


def runTest(testPath):
    testFile = file(testPath, "r")

    term = Fvterm.init(24, 80)
    flags = {}
    errors = 0

    for lineno, line in enumerate(testFile):
        lineno += 1
        lr = TestRunner(line, flags)
        try:
            lr.doCommand(term)
        except CheckFailed, e:
            print "%s:%d: %s" % (testPath, lineno, e)
            errors += 1

    if errors > 0: lr.do_DUMP(term)

    term.free()
    return errors

if __name__ == '__main__':
    errors = 0
    tests = 0
    Fvterm.loadlib(sys.argv[1])
    for arg in sys.argv[2:]:
        errors += runTest(arg)
        tests += 1
    if errors == 0:
        print "%d tests passed" % tests
        sys.exit(0)
    else:
        print "%d error(s)" % errors
        sys.exit(1)
