# Test of WRAP AROUND mode setting.

RES 80 24

IN ^[[?7h
REP 170 IN *

IN ^[[?7l^[[3;1H
REP 177 IN *

IN ^[[?7h^[[5;1HOK

SEQ 0 79 OUT 0 \# *
SEQ 0 79 OUT 1 \# *
SEQ 0 79 OUT 2 \# *
SEQ 0 79 OUT 3 \# \s
OUT 4 0 OK
