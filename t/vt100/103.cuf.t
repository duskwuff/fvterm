RES 80 24

# Markers, as usual
IN ^J^J^J^J
IN xxxx
CURSOR 4 4

IN ^[[C
CURSOR 4 5

IN ^[[99C
CURSOR 4 79

# Make sure that CUF resets wrapnext
IN !
IN ^[[C
IN !
CURSOR 4 79
OUT 4 79 !
IN !
CURSOR 5 1

# vim: set syn=conf:
