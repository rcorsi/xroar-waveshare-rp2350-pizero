' PIZERO-215: the CoCo 3's ALT, CTRL, F1 and F2, read straight from the
' keyboard matrix (row 6, columns 3-6) by a machine-code routine, since
' BASIC's BREAK check rescans the keyboard between statements. Hold each
' key: its name shows while held. Type in with scripts/serial_type.py.
NEW
5 CLEAR 200,&H7EFF
6 FOR I=0 TO 48:READ A:POKE &H7F00+I,A:NEXT
10 WIDTH 32:CLS:PRINT "HOLD ALT, CTRL, F1, F2"
15 N$="ALT CTRLF1  F2  "
20 EXEC &H7F00:FOR C=0 TO 3:P=64+32*C
25 IF (PEEK(&H7F40+C) AND 64)=0 THEN PRINT@P,MID$(N$,C*4+1,4) ELSE PRINT@P,"    "
30 NEXT:GOTO 20
40 DATA 26,80,198,247,247,255,2,182,255,0,183,127,64
41 DATA 198,239,247,255,2,182,255,0,183,127,65
50 DATA 198,223,247,255,2,182,255,0,183,127,66
51 DATA 198,191,247,255,2,182,255,0,183,127,67,28,175,57
RUN
