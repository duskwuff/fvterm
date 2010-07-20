RES 80 24

# Throw down a marker
IN x^M
OUT 0 0 x
CURSOR 0 0

# Down ten lines
IN ^J^J^J^J^J
IN ^J^J^J^J^J
CURSOR 10 0

# Up one
IN ^[[A
CURSOR 9 0

# Up one (again)
IN ^[[1A
CURSOR 8 0

# Did you know that 0 = 1?
IN ^[[0A
CURSOR 7 0

# Up all the way
IN ^[[99A
CURSOR 0 0

# Marker didn't move
OUT 0 0 x

# vim: set syn=conf:
