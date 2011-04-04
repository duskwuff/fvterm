# Various basic movement commands and DECALN

RES 80 24

IN ^[#8

IN ^[[9;10H^[[1J
IN ^[[18;60H^[[0J^[[1K
IN ^[[9;71H^[[0K

SEQ 10 16 IN ^[[\#;10H^[[1K^[[\#;71H^[[0K

IN ^[[17;30H^[[2K

SEQ 1 80 IN ^[[24;\#f*^[[1;\#f*

IN ^[[2;2H

REP 22 IN +^[[1D^[D

IN ^[[23;79H
REP 22 IN +^[[1D^[M

IN ^[[2;1H
SEQ 2 23 IN *^[[\#;80H*^[[10D^[E

IN ^[[2;10H^[[42D^[[2C
REP 76 IN +^[[0C^[[2D^[[1C

IN ^[[23;70H^[[42C^[[2D

REP 76 IN +^[[1D^[[1C^[[0D^H

IN ^[[1;1H
IN ^[[10A
IN ^[[1A
IN ^[[0A
IN ^[[24;80H
IN ^[[10B
IN ^[[1B
IN ^[[0B
IN ^[[10;12H

REP 58 IN \20
IN ^[[1B^[[58D

REP 58 IN \20
IN ^[[1B^[[58D

REP 58 IN \20
IN ^[[1B^[[58D

REP 58 IN \20
IN ^[[1B^[[58D

REP 58 IN \20
IN ^[[1B^[[58D

REP 58 IN \20
IN ^[[1B^[[58D

IN ^[[5A^[[1CThe screen should be cleared,  and have an unbroken bor-
IN ^[[12;13Hder of *'s and +'s around the edge,   and exactly in the
IN ^[[13;13Hmiddle  there should be a frame of E's around this  text
IN ^[[14;13Hwith  one (1) free position around it.    Push <RETURN>


# This is all pretty verbose and redundant... but whatever.

OUT  0  0 ****************************************
OUT  0 40 ****************************************
OUT  1  0 *+++++++++++++++++++++++++++++++++++++++
OUT  1 40 +++++++++++++++++++++++++++++++++++++++*
OUT 22  0 *+++++++++++++++++++++++++++++++++++++++
OUT 22 40 +++++++++++++++++++++++++++++++++++++++*
OUT 23  0 ****************************************
OUT 23 40 ****************************************

SEQ  2  7 OUT \#  0 *+\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
SEQ 16 21 OUT \#  0 *+\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
SEQ  2  7 OUT \# 20 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
SEQ 16 21 OUT \# 20 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
SEQ  2  7 OUT \# 40 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
SEQ 16 21 OUT \# 40 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
SEQ  2  7 OUT \# 60 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20+*
SEQ 16 21 OUT \# 60 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20+*

OUT  8  0 *+\20\20\20\20\20\20\20\20
OUT  9  0 *+\20\20\20\20\20\20\20\20
OUT 10  0 *+\20\20\20\20\20\20\20\20
OUT 11  0 *+\20\20\20\20\20\20\20\20
OUT 12  0 *+\20\20\20\20\20\20\20\20
OUT 13  0 *+\20\20\20\20\20\20\20\20
OUT 14  0 *+\20\20\20\20\20\20\20\20
OUT 15  0 *+\20\20\20\20\20\20\20\20

OUT  8 70 \20\20\20\20\20\20\20\20+*
OUT  9 70 \20\20\20\20\20\20\20\20+*
OUT 10 70 \20\20\20\20\20\20\20\20+*
OUT 11 70 \20\20\20\20\20\20\20\20+*
OUT 12 70 \20\20\20\20\20\20\20\20+*
OUT 13 70 \20\20\20\20\20\20\20\20+*
OUT 14 70 \20\20\20\20\20\20\20\20+*
OUT 15 70 \20\20\20\20\20\20\20\20+*

OUT  8 10 EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
OUT 15 10 EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE

OUT  9 10 E\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
OUT 14 10 E\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
OUT  9 30 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
OUT 14 30 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20
OUT  9 50 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20E
OUT 14 50 \20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20\20E

OUT 10 10 E The screen should be cleared,  and have an unbroken bor- E
OUT 11 10 E der of *'s and +'s around the edge,   and exactly in the E
OUT 12 10 E middle  there should be a frame of E's around this  text E
OUT 13 10 E with  one (1) free position around it.    Push <RETURN>  E

CURSOR 13 67
