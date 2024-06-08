;--------------------------------------
; Applewin HD Controller IOs
;--------------------------------------
IO_Execute 		 = $c080				; R
IO_Status 		 = $c081				; R : b7=busy, b0=error
;--------------------------------------
IO_Command 		 = $c082				; W : BLK = $00 status, $01 read, $02 write. SP = $80 status, $81 read, $82 write,
IO_Unitnum 		 = $c083				; W : BLK = DSSS0000 if SSS != n from CnXX, add 2 to D (4 drives support). SP = $00,$01.....
IO_MemPtr 		 = $c084				; W : 16 bits values
;--------------------------------------
; to be added :
IO_StatusCode	 = $c086				; W : write SP status code $00, $03
;(see hddrvr-v3.a65 : SP_Commands)
;--------------------------------------
IO_BlockNum		 = $c086				; W : BLK = 16 bits values. SP = 24bits values, see below
;--------------------------------------
; to be added :
IO_BlockNum24	 = $c088				; W : 3rd byte for SP 24 bits blocknum (same as IO_Nextbyte but W, not R )
;--------------------------------------
IO_Nextbyte		 = $c088				; R : legacy read-only port (still supported by AppleWin)
IO_Imgsizelo	 = $c089				; R
IO_Imgsizehi	 = $c08a				; R
;--------------------------------------
; Notes on accesses to I/O registers:
; . ROR ABS16,X and ROL ABS16,X - only used for $C081+s*$10 STATUS register:
;     6502:  double read (old data), write (old data), write (new data). The writes are harmless as writes to STATUS are ignored.
;     65C02: double read (old data), write (new data). The write is harmless as writes to STATUS are ignored.
; . STA ABS16,X does a false-read. This is harmless for writable I/O registers, since the false-read has no side effect.
;--------------------------------------


