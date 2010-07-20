RES 80 24

# Throw down a marker
IN x^M
OUT 0 0 x
CURSOR 0 0

# Down ten lines
IN ^J^J^J^J^J
IN ^J^J^J^J^J
CURSOR 10 0

# Down one
IN ^[[B
CURSOR 11 0

# Down one (again)
IN ^[[1B
CURSOR 12 0

# Down all the way
IN ^[[99B
CURSOR 23 0

# And the marker didn't move
OUT 0 0 x

# vim: set syn=conf:
