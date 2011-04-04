# Test of cursor-control characters inside ESC sequences

RES 80 24

IN A B C D E F G H I
IN ^M^J
IN A^[[2^HCB^[[2^HCC^[[2^HCD^[[2^HCE^[[2^HCF^[[2^HCG^[[2^HCH^[[2^HCI
IN ^M^J
IN A ^[[^M2CB^[[^M4CC^[[^M6CD^[[^M8CE^[[^M10CF^[[^M12CG^[[^M14CH^[[^M16CI
IN ^M^J
IN A ^[[1^KAB ^[[1^KAC ^[[1^KAD ^[[1^KAE ^[[1^KAF ^[[1^KAG ^[[1^KAH ^[[1^KAI ^[[1^KA

SEQ 0 3 OUT \# 0 A B C D E F G H I
CURSOR 3 18
