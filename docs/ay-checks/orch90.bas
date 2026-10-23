' PIZERO-148: Orchestra-90CC test (two 8-bit DACs, left $FF7A, right $FF7B).
' BASIC buzzes the left DAC, then the right; then a machine-code loop at $7000
' plays two sawtooth waves at once, about 500 Hz on the left and 830 Hz on the
' right, writing each DAC about 42,000 times a second for about 1.5 seconds:
'   LDX #$FFFF / CLRA / CLRB / loop: STA $FF7A / STB $FF7B / ADDA #3 / ADDB #5
'   LEAX -1,X / BNE loop / RTS
10 CLS:PRINT"ORCHESTRA-90: TWO DACS"
20 PRINT"LEFT":FOR I=1 TO 300:POKE &HFF7A,255:POKE &HFF7A,0:NEXT
30 PRINT"RIGHT":FOR I=1 TO 300:POKE &HFF7B,255:POKE &HFF7B,0:NEXT
40 CLEAR 200,&H6FFF:FOR A=&H7000 TO &H7013:READ B:POKE A,B:NEXT
50 PRINT"BOTH, FROM MACHINE CODE":EXEC &H7000:PRINT"DONE"
60 DATA 142,255,255,79,95,183,255,122,247,255,123,139,3,203,5,48,31,38,242,57
