 ORG $2000 ;Re-ORG as necessary
 LST ON,S,ASYM,VSYM
 SBTL 'DISK FORMAT & PREPARATION ROUTINE'
 REP 50
*
*       APPLE'S PRODOS FORMATTING/DISK BUILDING MODULE
* 
*                  VERSION 1.0.3
*           
*
*       MODIFIED FOR COMPLIANCE WITH PRODOS TECHNOTE #16
*       AND, TO CONSOLIDATE BUILDDISK, AND FORMATTER.
*       INTO ONE MODULE, ACCESSED WITH A SINGLE CALL.
*       ALSO TO PROVIDE A MECHANISM FOR THE USER TO
*       DETERMINE THE NUMBER OF BLOCKS ON THE DEVICE,
*       AND WHETHER IT IS A DISK II, /RAM, OR OTHER.
*                          ON 10/17/85, BY PETE MC DONALD.
*
*       COPYRIGHT APPLE COMPUTER, INC., 1982-85
*
 REP 50
*
* ENTRY1: (ORG+$10) 
*   INITIALIZES THE SPECIFIED UNIT #. BY:
*     -FORMATTING PHYSICAL MEDIA. IF IT CAN BE FORMATTED
*     -WRITING BOOT LOADER IN BLOCKS 0,1 (EVEN IF IT IS 
*      NOT A DISK II).
*     -CREATING VOLUME DIRECTORY IN BLOCKS 2,3,4,5 WITH 
*      THE NAME CONTAINED IN THE FIRST 16 BYTES OF THE MODULE.
*     -ALLOCATING BIT MAP IN BLOCKS 6 AND UP.
*
*   ON ENTRY, @ ENTRY1 (ORG+$10):
*     A   = THE UNIT NUMBER, IN THE FORMAT REQUIRED BY THE
*           PRODOS WRITE BLOCK CALL.
*     X/Y = THE ADDRESS OF A BUFFER OF AT LEAST 512 BYTES
*           WHICH THIS ROUTINE CAN FREELY USE.  X = THE
*           LOW PART OF THE ADDRESS.
*     THE FIRST 16 BYTES OF THE MODULE MUST CONTAIN THE DESIRED
*           VOLUME NAME, WHERE THE FIRST BYTE CONTAINS THE LENGTH
*           OF THE NAME (SHOULD BE 1 THRU 15) AND THE REMAINING 15 
*           BYTES CONTAIN THE NAME (SHOULD BE A PROPER VOLUME
*           NAME, WITH NO LEADING SLASH).  THE ORIGINAL
*           CONTENTS OF THESE 16 BYTES ARE 12 (THE LENGTH), AND THE
*           NAME 'DEFAULT.NAME   '.  NO CHECKING IS DONE ON THESE
*           16 BYTES, BUT BIT 7 IS STRIPPED FROM EACH BYTE OF THE
*           NAME.
*
*
*   ON RETURN:
*     Z IS UNDEFINED.
*     C = THE ERROR STATUS:
*         C = 0 INDICATES NO DETECTED ERRORS.
*         C = 1 INDICATES AN ERROR, IN WHICH CASE THE ACCUMULATOR
*               CONTAINS THE PRODOS ERROR CODE.
*     A = 0 IF THERE WAS NO ERROR (C = 0), OR IT CONTAINS THE
*         PRODOS ERROR CODE IF THERE WAS AN ERROR (C = 1).
*     X AND Y ARE DESTROYED.
*     THE FIRST 512 BYTES OF THE BUFFER ARE DESTROYED.
*     0-PAGE LOCATIONS 2 AND 3 HAVE BEEN USED, BUT HAVE BEEN RESTORED
*         TO THEIR VALUES AT ENTRY.
*     0-PAGE LOCATIONS $42-$47 HAVE BEEN CHANGED AND NOT RESTORED.
*
*   IF CALLED FOR A NON-EXISTENT DISK (INCLUDING SLOT 0), 
*   ERROR $28 WILL BE RETURNED.
*   IF IT IS AN EXISTENT UNIT NUMBER, THE PROCESS BEGINS,
*   AND OTHER ERRORS ARE THEN POSSIBLE.
*  
*   USES UNIT, ROM ID'S, AND DRIVER ADDRESS TO DETERIME SIZE
*   THOSE DEVICES FOR WHICH THE SIZE IS GOTTEN BY
*   A STATUS CALL TO THE SLOT'S ROM, THE DRIVE BIT IS 
*   PRESENT IN LOCATION $43 AND IS USED TO INDEX INTO THE 
*   DRIVER VECTOR TABLE.  THEREFORE, THE 2 DRIVES IN A SLOT 
*   COULD BE DIFFERENT SIZES.
*
 page
 REP 50
*
* ENTRY2 (ORG+$13):
* RETURNS THE SIZE AND WHETHER IS DISK II, /RAM, OR OTHER.
*
* ON ENTRY, @ ENTRY2 (ORG+$13):
* A = UNIT NUMBER, IN FORMAT REQUIRED BY PRODOS.
* X/Y = THE ADDRESS OF A BUFFER OF AT LEAST 512 BYTES,
*       WHICH THIS ROUTINE CAN USE FREELY. X = THE
*       LOW PART OF THE ADDRESS.
*
*
* ON RETURN:
*   Z IS UNDEFINED.
*   C = THE ERROR STATUS.
*       C = 0, NO ERROR
*       C = 1, AN ERROR WAS DETECTED.
*   X = (SIZE lo) IF C = 0, X = UNDEFINED IF C = 1
*   Y = (SIZE HI) IF C = 0. IF C = 1 THEN y = undefined
*   A = error code if C = 1: IF C = 0 then 
*   A = 0 FOR DISK II, A = 1 FOR /RAM, A  = 2 FOR ALL OTHERS.
*
* THE FIRST 512 BYTES OF THE BUFFER ARE DESTROYED.
* ZERO PAGE LOCS 2 AND 3 HAVE BEEN USED, BUT HAVE
* BEEN RESTORED TO THEIR VALUES AT ENTRY.
* 0 - PAGE LOCATIONS 42-47, HAVE BEEN CHANGED AND 
* NOT RESTORED.
*
* IF CALLED FOR NON EXISTENT DEVICE, ERROR $28 WILL BE RETURNED
* IF CALLED FOR EXISTENT DEVICE, PROCESS WILL BEGIN, AND THEN
* OTHER ERRORS ARE POSSIBLE.
*
* USES UNIT, ROM ID'S, AND DRIVER ADDRESS TO DETERIME SIZE
* THOSE DEVICES FOR WHICH THE SIZE IS GOTTEN BY
* A STATUS CALL TO THE SLOT'S ROM, THE DRIVE BIT IS 
* PRESENT IN LOCATION $43 AND IS USED TO INDEX INTO THE 
* DRIVER VECTOR TABLE.  THEREFORE, THE 2 DRIVES IN A SLOT 
* COULD BE DIFFERENT SIZES.
*
 page
 REP 50
*
MLIENTRY EQU $BF00 ;MLI ENTRY POINT 
DEVADRS01 EQU $BF10 ;DEVICE DRIVER VECTOR TABLE
DEVCNT EQU $BF31 ;ONLINE DEVICE COUNT
DEVLST EQU $BF32 ;ONLINE DEVICE LIST
DATELO EQU $BF90 ;DATE/TIME (4 BYTES)
KMVERSN EQU $BFFE ;MINIMUM VERSION #
KVERSION EQU $BFFF ;PRODOS VERSION
ENTRYB EQU $0D ;ENTRIES/BLOCK IN A DIRECTORY
ENTRYL EQU $27 ;DIRECTORY ENTRY LENGTH
*
* PAGE ZERO LOCATIONS
*
BUFFPTR EQU 2 ;AND 3
DEVCMD EQU $42
DEVUNIT EQU $43
DEVBLK EQU $46 ;AND 47
*
BUILDDISK EQU *
*
DUMSIZE DB 12 ;SIZE OF THE DUMMY NAME
DUMMYNAM ASC 'DEFAULT.NAME   '
 page
*
* ACTUAL ENTRY POINTS ARE HERE
*
entry1 jmp form ;normal builddisk entry
entry2 jmp stat ;return device size and type.
*
* this performs the normal format and disk build operations
*
form jsr validate ;save regs/verify unit num
 bcs out ;error go home zp restored by validate
 jmp Build ;else do format/build
*
* returns device size in x/y y=hi, and a= 0 for DISK II,
* 1 for /RAM, 2 for all others./
*
stat jsr validate ;save zp/validate unit #
 bcs out ;error go home/zp restored by validate.
 jsr getblks ;else get and return size/type of unit
 stx loctemp ;save size hi for a sec
 jsr error ;no error, but fix zp locs 2,3
 tax  ;for the sake of consistency swap
 tya  ;regs around to conform with normal
 ldy loctemp ;status call.
out rts  ;bye
*
Validate STA UNITNUM1 ;SAVE THE PASSED UNIT NUMBER
 STA UNITNUM2 ;SAVE THE PASSED UNIT NUMBER
 STA UNITNUM3 ;SAVE THE PASSED UNIT NUMBER
*
 AND #$F0 ;CLEAR LO BITS JUST IN CASE
 STA UNITLEFT ;SAVE FOR LATER (LEFT JUSTIFIED)
*
 LDA BUFFPTR ;SAVE CURRENT CONTENTS OF 0-PAGE LOCATIONS
 STA SAVEZERO
 LDA BUFFPTR+1
 STA SAVEZERO+1
*
 STX BUFFPTR ;SAVE PASSED BUFFER ADDRESS IN 0-PAGE
 STY BUFFPTR+1
 STX LBUFFPTR ;ALSO SAVE IN LOCAL PARAMETER LIST
 STY LBUFFPTR+1
*
 LDX DEVCNT ; POINT TO LAST ACTIVE DISK
LOOP LDA DEVLST,X ; GET BYTE FOR ACTIVE DISK
 AND #$F0 ; CLEAR OUT DISKTYPE INDICATOR
 CMP UNITLEFT ; FOUND OUR DISK?
 BEQ FOUND ; YES
 DEX ; NO, POINT TO NEXT DISK
 BPL LOOP ; IF X HAS NOT GONE NEGATIVE, THERE ARE MORE DISKS
*
* DIDN'T FIND DISK.
 LDA #$28 ; SIGNAL NO DEVICE
 SEC ; SIGNAL ERROR
 BCS ERROR ;ALWAYS
found clc 
 RTS
*
BUILD equ *
 jsr doformat ;go format if can
 bcs error ;taken if error during format.
 jsr isitram ;is unitnum /RAM ?o-SD
 BNE OKDISK ;NO
*
 LDA #0 ;No error, but do nothing              |
 CLC ;  since RAMDISK Format does it for us |
 BCC ERROR ;ALWAYS                                |
*
okdisk equ * ;mod
 lda UNITLEFT ;*mod* recover disktype
 STA DISKTYPE ;SAVE FOR LATER
 JSR MLIENTRY ;WRITE BOOT BLOCK 0
 DB $81
 DW BLK0PARM
 BNE ERROR
*
 JSR MLIENTRY ;WRITE BOOT BLOCK 1
 DB $81
 DW BLK1PARM
 BNE ERROR
*
 JSR ROOTBLK2 ;WRITE ROOT DIRECTORY BLK 2
 BCS ERROR
 JSR ROOTBLKS ;WRITE ROOT DIRECTORY BLKS 3-5 
 BCS ERROR
 JSR BITMAPS ;WRITE BIT MAP IN BLKS 6 AND UP
*
ERROR LDX SAVEZERO ;RESTORE 0-PAGE LOCATIONS
 STX BUFFPTR
 LDX SAVEZERO+1
 STX BUFFPTR+1
*
 RTS
*
SAVEZERO DW 0
DISKTYPE DB 0 ;LOW ORDER NIBBLE FROM DEVLST
*
BLK0PARM DB 3 ;3 PARAMETERS
UNITNUM1 DB 0 ;UNIT NUMBER FILLED IN FROM CALLER
 DW BOOTZERO ;ADDRESS OF BOOT BLOCK
 DW 0 ;WRITE TO BLOCK 0
*
BLK1PARM DB 3 ;3 PARAMETERS
UNITNUM2 DB 0 ;UNIT NUMBER FILLED IN FROM CALLER
 DW BOOTONE ;ADDRESS OF BOOT BLOCK
 DW 1 ;WRITE TO BLOCK 1
 SBTL 'SETUP & WRITE ROOT DIRECTORY'
ROOTBLK2 EQU *
 JSR MLIENTRY ;GET DATE/TIME
 DB $82
 DW 0
*       CANNOT GENERATE AN ERROR FROM THIS CALL
 JSR CLRBUFF ;CLEAR BUFFER
 LDY #2 ;POINT TO FORWARD POINTER IN BUFFER 
 LDA #3 ;BLOCK 2 POINTS FORWARD TO BLOCK 3
 STA (BUFFPTR),Y
*
 LDY #4+0 ;POINT TO STORAGE TYPE/NAME LENGTH
 LDA DUMSIZE ;USE DUMMY NAME
 ORA #$F0 ;OR IN THE DIRECTORY MARKER
 STA (BUFFPTR),Y
 LDX #0 ;POINT TO FIRST CHARACTER OF NAME
RTD1 EQU *
 INY
 LDA DUMMYNAM,X ;GET VOLUME NAME
 AND #$7F ;NO HI BIT
 STA (BUFFPTR),Y ;WRITE TO BUFR
 INX
 CPX DUMSIZE ;DONE?
 BNE RTD1 ;NO
*
 LDX #0 ;POINT TO DATE/TIME IN PRODOS
 LDY #4+$18 ;POINT TO DATE/TIME IN BUFFER
RT2 LDA DATELO,X ;TRANSFER DATE/TIME TO BUFFER
 STA (BUFFPTR),Y
 INX
 INY
 CPX #4
 BNE RT2 
*
 LDA KVERSION ;GET VERSION # FROM GLOBALS
 STA (BUFFPTR),Y
*
 INY  
 LDA KMVERSN ;AND MINIMUM VERSION #
 STA (BUFFPTR),Y
 INY
 LDA #$C3 ;GET ACCESS BYTE VALUE
 STA (BUFFPTR),Y
 INY
 LDA #ENTRYL ;GET ENTRY LENGTH
 STA (BUFFPTR),Y
 INY
 LDA #ENTRYB ;GET ENTRIES/BLOCK
 STA (BUFFPTR),Y
 LDY #4+$23 ;POINT TO BITMAP PTR
 LDA #6 
 STA (BUFFPTR),Y
 JSR GETBLKS ;GET TOTAL BLKS (A=LO,X=HI)
 BCS RTRET ;QUIT ON ERROR
 LDY #4+$25 ;POINT TO TOTAL BLOCKS IN BUFFER
 STA (BUFFPTR),Y ;LO
 STA TEMP ;ALSO STORE IN TEMP AND TEMP1
 INY
 TXA
 STA (BUFFPTR),Y ;HI
 STA TEMP1
*
 LDA #1 ;SET UP TO WRITE BLOCK 2 (GETS INCREMENTED)
 STA BLKNUM
 JSR WRTBLK
RTRET RTS ;C=1 MEANS ERROR ON WRITE, ELSE GOOD
*
TEMP DB 0
TEMP1 DB 0
 PAGE
 REP 50
*
* isitram - determine if chosen unit, is /ram
*
* if vector is $ff00, then is /ram.
*
* ENTRY: unitleft contains left justified unitnum
* EXIT: X unchanged, a,y scrambled
* is ram, z=1
* not ram z=0
*
isitram equ *
 lda unitleft ;get justified unit number
 lsr a ;right justify
 lsr a
 lsr a
 jsr getvect ;get device driver address
 bne notram ;taken if low order of vect not 0
 cpy #$FF ;is high byte $FF
 bne notram ;if not is not /ram and z=0  return
 lda #00 ;is /ram make z=1
notram rts  ;bye
*
*
* isitdiskII - is unit number a disk II
* use signature bytes in CN00
* derive cn from unitleft
* ENtry: unitleft contains left just unitnum
* exit : z=1 is diskII
*        z=0 not disKII
* a,x scramble
* y unchanged 
*
isitdiskII equ *
 lda unitleft ;get unit number
 and #$70 ;extract slot number
 lsr a ;move slot to low nibble.
 lsr a
 lsr a
 lsr a
 ora #$c0 ;append $C for a slot address
 sta loadd+2 ;mod hi byte of lda to point to CNXX
 ldx #3 ;init count of bytes to check
chklp lda dIIlocs,x ;get lowbyte of address to check
 sta loadd+1 ;save at load instruction
loadd lda $C000 ; self modifying  get byte at loc
 cmp dIIids,x ;is it a DiskII signature ?
 bne endDII ;taken if no
 dex  ;else check the next one
 bpl chklp ;are there anymore ?
 inx  ;make z=1 
endDII rts  ;bye z=0 is not DII, z=1, is dII
dIIids dfb $20,0,3,0 ;DiskII signatures,
dIIlocs dfb 1,3,5,$ff ;and thier respective locations. 
*
*
* DOFORMAT-format physical disk media if possible
*
* If disk II, use formatter driver.
* If ram based. Or if Rom based with can format true, use 
* device format call.
* If is Rom based, and can format is not true, then do nothing.
* called only after it has been determined that unit # is valid
*
*       ENTRY: BUFFPTR, & BUFFPTR+1, are user passed buffer address.
*              UNITLEFT contains left justified unit num.
*              UNITNUM1, contains originally passed unit num.
*              A=unitnum, X&Y=don't care.
*
*       EXIT: Success a=0 c=0 x,y unknown.
*             Failure, a=errnum, c=1, x,y unknown.
*             if ROM call made, $42-$47, destroyed.
* 
*
doformat equ *
 jsr isitdiskII ;is unit a disk II
 bne dforcont ;taken if not
 ldx #$D ;init index/count-1
savlp lda $d0,x ;save locs $D0 - $DD
 sta locbuff,x
 dex  ;next one /any more ?
 bpl savlp ;taken if yes
 lda UNITLEFT ;get the original unit num
 jsr FORMATTII ;format a disk II
 php  ;save error status
 pha  ;save error code (if any)
 ldx #$D ;restore $d0 - $DD
reslp lda locbuff,x
 sta $D0,x
 dex  ;next one /anymore ?
 bpl reslp ;taken if yes.
 pla  ;recover Error code
 plp  ;and error status.
 rts  ;done bye
*
dforcont lda unitleft ;get unit num again
 lsr a ;right justify
 lsr a  
 lsr a
 jsr getvect ;get vector to device driver.
 sta devjmp ;save lowbyte 
 sty devjmp+1 ;and high byte for driver call.
 tya  ;get high byte of address
 and #$f0 ;clear low nibble
 cmp #$c0 ;does it point to $CN ?
 bne dforcont1 ;taken if no.(is not ROM based)
*
 sty modadd+2 ;set up LDA to point at attributes
modadd lda $C0FE ;modified by previous instruction.
 and #$08 ;isolate can format bit.
 bne dforcont1 ;taken if it supports formatting
 clc  ;does not, but no error, just return for
 rts  ;builddisk to write dir etc.
*
dforcont1 lda unitnum1 ;get back unit num
 sta devunit ;save it for driver
 lda #00 ;zero for
 sta devblk ;block
 sta devblk+1 ;and block hi
 lda #03 ;get format command
 sta devcmd ;save it for driver
 sta $cfff ;turn off whoever has $c800
 lda $c08b ;switch language card in
 lda $c08b
 jsr asjmp ;jsr to jmp indirect to driver.
 bit $c082 ;switch rom back in.
 rts  ;ret to continue build, with error if any
*
*  GETBLKS - GET # OF BLOCKS ON DEVICE. 
*
*  IF IT'S A DISKII, BLOCKS ARE HARDCODED AT $118.  IF
*  is /RAM # blocks hardcoded at $7f. If is is an 
*  OTHER KIND OF DISK, NUMBER OF BLOCKS MAY BE IN
*  $CNFC,FD.  BUT IF THEY CONTAIN 0, THEN MUST CALL 
*  ROM CODE WITH STATUS REQUEST TO GET BLOCKS.
*
*  ENTRY:  UNITNUM1, UNITLEFT, AND DISKTYPE MUST BE SET.
*  EXIT: Success: Carry = 0,  X=HI,A=LO BLOCKS
*               : Y=0 for DISK II, Y=1 for /RAM, Y=2 for all others.
*        Failure: Carry = 1, Accum = error code
*               : X and Y undefined
*        If ROM call is made: $42-$47 destroyed
*
 REP 50
*
GETBLKS equ * ;was LDA DISKTYPE
 jsr isitdiskII ;was CMP #DISKII
 BNE DB1a ;NO
 lda #0 ;save the fact that this is disk II
 sta loctemp ;at a local temp
 LDA #$18 ;YES, SIZE = 280 BLOCKS
 LDX #1
 BNE DB2 ;ALWAYS
*
db1a equ *
 jsr isitram ;is this /RAM ?
 bne db1 ;taken if not
 lda #1 ;save the fact that tis is /RAM
 sta loctemp ;at a local temp
 ldx #0 ;and there are $7f blocks
 lda #$7f ;get them and reset z
 bne db2 ;alwayss
*
* Not DISK II SO CHECK $CNFC-FD
*
DB1 LDA UNITLEFT ; get unit number (left justified)     |
 LSR A ; SHIFT UNIT NUMBER INTO TABLE INDEX   |
 LSR A ;                                      |
 LSR A ;                                      |
 STA UNITRIGHT ; save it                              |
 JSR GETVECT ;GET DRIVER VECTOR FROM TABLE          |
*
* mod to check if driver is in CN00, if not must ask size.
*
 tya
 and #$F0 ;clear all but high nibble of address
 cmp #$c0 ;is it in rom space
 bne ask ;taken if not in ROM
*
*
 STY BUFFPTR+1 ;HI                                    |
 LDA #$FC
 STA BUFFPTR ;LO
 LDY #0
 LDA (BUFFPTR),Y
 PHA
 INY 
 LDA (BUFFPTR),Y
 TAX
 BNE DB3 ; if not zero, then size is hardwired
 PLA  ; get lo byte
 BNE DB2 ; if not zero then return
ask JSR ASKSIZE ; need to go ask device for size
 BCS RESTORE ; branch if error
 lda #2 ;save the fact that this is a "other"
 sta loctemp ;at a local temp
 TXA  ; get low byte
 PHA  ; save it
 TYA  ; get hi byte
 TAX  ; put in X
 PLA  ; get low byte back
 bcc RESTORE ; GO RESTORE BUFFER POINTER AND EXIT
*
DB3 PLA
DB2 CLC  ; clear carry to indicate success
RESTORE LDY LBUFFPTR ; RESTORE THE POINTER TO THE CALLER'S BUFFER
 STY BUFFPTR
 LDY LBUFFPTR+1
 STY BUFFPTR+1
 ldy loctemp ;get the kind of device anbd return it.
 RTS
*
ASKSIZE EQU * ; ask device for size via status command
 LDA #0 ; COMMAND BYTE = 0 (give status)
 STA DEVCMD ; store in zero page parameters
 STA DEVBLK ; block #  
 STA DEVBLK+1
 LDA UNITRIGHT ; get unit # (right justified)
 JSR GETVECT ;                                       |
 STA DEVJMP ;                                       |
 STY DEVJMP+1 ;                                       |
 LDA UNITNUM1 ; get unit #, with possible low nibble junk
 STA DEVUNIT
 STA $CFFF
 lda $c08b
 lda $c08b
 JSR ASJMP 
 bit $c082
 RTS
ASJMP JMP (DEVJMP) ; indirect jump to ROM entry
 BRK
*
GETVECT EQU * ;                                     |
 TAY ;VECTOR INTO TABLE IN ACC ON ENTRY    |
 LDA DEVADRS01,Y ;GET LOW BYTE                         |
 PHA ;SAVE IT                              |
 LDA DEVADRS01+1,Y ;GET HI BYTE                          |
 TAY ;SAVE IT IN Y                         |
 PLA ;GET LO BYTE BACK                     |
 RTS ;                                     |
*
UNITLEFT DB 0 ; UNIT NUMBER
UNITRIGHT DB 0 ; reserve a byte for saving slot
DEVJMP DW 0 ; ADDRESS IN ROM
*
 REP 50
*
*  WRTBLK -- INCREMENTS THE BLOCK NUMBER AND WRITES
*            CURRENT BUFFER TO THAT BLOCK
*
 REP 50
*
WRTBLK EQU *
 INC BLKNUM
 JSR MLIENTRY
 DB $81 ;BLOCK WRITE
 DW PARMLIST
 RTS
*
PARMLIST EQU * ;PARAMETER LIST FOR WRITING BLOCKS 2 AND UP
 DB 3 ;PARM COUNT
UNITNUM3 DB 0
LBUFFPTR DW 0
BLKNUM DW 0
 PAGE
 REP 50
*
*  ROOTBLKS - SETUP AND WRITE FORWARD & BACKWARD POINTERS
*  INTO BLOCKS 3,4,5 OF ROOT DIRECTORY
*
 REP 50
*
ROOTBLKS EQU *
 JSR CLRBUFF ;CLEAR OUT BUFFER AREA
 LDA #2 ;BACKWARD POINTER FOR BLOCK 3
 LDX #4 ;FORWARD POINTER FOR BLOCK 3
 JSR BF1 ;WRITE BLOCK 3 TO DISK
 BCS BF2 ;ERR
 LDA #3 ;BACKWARD POINTER FOR BLOCK 4
 LDX #5 ;FORWARD POINTER FOR BLOCK 4
 JSR BF1 ;WRITE BLK 4 TO DISK
 BCS BF2 ;ERR
 LDA #4 ;BACKWARD POINTER FOR BLOCK 5
 LDX #0 ;FORWARD POINTER FOR BLOCK 5
BF1 LDY #0 ;POINT TO BACKWARD POINTER
 STA (BUFFPTR),Y ;STORE BACKWARD PTR IN BUFFER
 LDY #2 ;POINT TO FORWARD POINTER
 TXA
 STA (BUFFPTR),Y ;STORE FORWARD PTR IN BUFFER
 JSR WRTBLK ;CALL MLI FOR WRITE TO DISK
BF2 RTS ;C=1 ERR ON WRITE ELSE OK
 SBTL 'SETUP AND WRITE BITMAP'
 REP 50
*
*  CONVBLKS - CONVERT # OF BLOCKS ON DEVICE TO # OF 
*  BYTES TO SET IN BITMAP FOR NEWLY FORMATTED DISK. 
*
*   ENTER: TEMP  = LO BLOCKS
*          TEMP1 = HI BLOCKS
*
*    EXIT: TEMP  = LO BYTES
*          TEMP1 = HI BYTES 
*          NBITS = EXTRA BITS
*
 REP 50
*
CONVBLKS EQU *
 LDA TEMP ; get low order # of blocks
 AND #$07 ; put remainder of bits in NBITS
 STA NBITS
 LDA TEMP ; mask out low bits
 AND #$F8 ;   so that ROR's work
 CLC  ; clear carry for first time
 LDX #3 ; slide bits down 3 places
CNV1 ROR TEMP1 ;   slide top bits down
 ROR A ;   slide bottom bits down
 DEX
 BNE CNV1 ;   go back and do more 
 STA TEMP ; put back lower bits
 RTS
*
NBITS DB 0
 PAGE
 REP 50
*
*  BITMAPS - SETUP AND WRITE BIT MAPS TO NEWLY FORMATTED
*  DISKS.  WORKS FOR ARBITRARY-SIZED VOLUMES. 
*
* INPUTS:
*    TEMP,TEMP1 -- word containing # blocks in device
*
* OUTPUTS:
*    TEMP,TEMP1 -- destroyed
*    CARRY --  0 -> success
*              1 -> failure, code in accumulator
* 
 REP 50
*
BITMAPS EQU *
 jsr convblks ;convert # blks to # of bytes & bits
 LDA NBITS ;ANY EXTRA BITS?
 BEQ BTMP1 ;BRANCH IF NOT
 INC TEMP ;DO 1 MORE BYTE IN BITMAP
 BNE BTMP1 ; IF THERE IS.
 INC TEMP1
BTMP1 EQU *
 lda temp1 ; hi byte of # of bytes in bit map 
 clc
 ror A ; get # of blocks in bit map (#/$200)
 bcs used ; if carry set, then need another bit
 ldx temp ; did we end on even block boundary?
 beq used ; branch if so (carry clear) 
 sec  ; otherwise, set carry for another bit
used adc #6 ; add in # bits for root dir & boot blks 
 pha  ; save # of bits to mark as used 
 LDA NBITS
 BEQ BTMP2
 DEC TEMP ;RESTORE # BYTES IN MAP TO # FULL BYTES
 LDA TEMP
 CMP #$FF
 BNE BTMP2
 DEC TEMP1
BTMP2 JSR CLRBUFF ;ZERO THE BUFFER
mapblock lda temp1 ; check # of bytes left TO DO 
 cmp #2 ; LESS THAN $200 BYTES to do?
 bcc last ; go do last block if YES. 
 SBC #2 ;SET COUNT TO DO AT N-$200
 STA TEMP1
 jsr setfull ; set the full block with $FF's 
 pla  ; get # of bits to mark used
 jsr markused
 jsr WRTBLK ; write this block of bit map out 
 bcs bterr ;   branch if error during write
 lda #0 ;# of bits to be marked as used next time
 pha
 beq BTMP2 ; always taken
last jsr CLRBUFF ; clear buffer first
 lda temp1 ; 1 PAGE OR LESS?
 beq last1 ;BRANCH IF YES
 LDA #$FF
 jsr WRTBITS ; set first page to $FF's
 inc BUFFPTR+1 ; point to second page 
LAST1 LDY TEMP ;ANY FULL BYTES ON THIS PG OF PARTIAL BLOCK?
 BEQ LAST1.1 ;BRANCH IF NOT
 jsr setCNT ; ELSE mark final partial block 
LAST1.1 LDY TEMP
 INY ;POINT TO BYTE AFTER TEMP
 JSR EXTRAB ;GO DO EXTRA BITS
 lda temp1 ; was it the in the 1st or 2nd page ? 
 beq marklast ; branch if 1st 
 dec BUFFPTR+1 ; fix BUFFPTR if we did 2nd page 
marklast pla  ; pull off # of bits to mark as used
 jsr markused ; mark them used
 jsr WRTBLK ; write out last block of bit map 
 rts  ; return (with carry set if error) 
*
markused equ * ; mark some bits as used at start of blk
 ldy #0 ; point to beginning of buffer
markloop tax  ; store # bits to mark in X
 beq markrtn ; if no bits to mark as used, return
 cpx #8 ; more than eight bits to mark?
 bcc markpart ; branch if less than full byte
 lda #0 ; zero out whole byte
 sta (BUFFPTR),y
 iny  ; point to next byte in block 
 txa  ; get # bits in accum
 sec  ; set carry for subtraction
 sbc #8 ; subtract off bits just marked
 bpl markloop ; always taken 
markpart lda #$7F ; at least 1 bit gets cleared 
partloop dex  ; are we done?
 beq partdone ; branch if so
 clc  ; otherwise, rotate in another clear bit
 ror A
 bcs partloop ; branch always
partdone sta (BUFFPTR),y
markrtn rts
*
bterr equ * ; return after error writing block 
 tax  ; save error code in X
 pla  ; pull off extraneous byte from stack 
 txa  ; get error code back 
 sec  ; mark as error 
 rts 
*
SETCNT EQU * ;SET # OF BYTES IN Y TO $FF
 LDA #$FF
SETCNT1 DEY
 STA (BUFFPTR),Y
 BNE SETCNT1
 RTS
*
extrab lda nbits ; write leftover bits  
 beq ex1 ; none left over 
 jsr getbits ; gives mask in acc, leaves Y intact 
 DEY
 sta (BUFFPTR),y 
ex1 rts 
*
CLRBUFF equ * ; clear buffer area 
 lda #0 ;                                      |
 BEQ WRTB1 ;                                      |
SETFULL LDA #$FF ;FILL BUFFER W/$FF                     |
WRTB1 jsr wrtbits ; write out bits of 00's OR $FF's      |
 inc BUFFPTR+1 ; set to 2nd page of buffer 
 jsr wrtbits 
 dec BUFFPTR+1 ; restore pointer 
 rts 
wrtbits ldy #0
wrtb sta (BUFFPTR),Y
 iny 
 bne wrtb 
 rts 
*
 REP 50
*
*   GETBITS - GET BIT MASK FOR REMAINING BITS.
*   USED IN FORMAT FOR SETTING BITS IN VOLUME 
*   BIT MAPS.
*
 REP 50
*
GETBITS EQU * ;GET BIT MASK FOR REMAINING BYTE IN UIMEM
 SEC
 LDA #$08
 SBC NBITS
 BEQ ALLBITS
 TAX 
 CPX #8
 BEQ NOBITS
 LDA #$FF
SHFBITS ASL A
 DEX 
 BNE SHFBITS
 BEQ SOMEBITS
ALLBITS LDA #$FF
SOMEBITS EQU *
 RTS
NOBITS LDA #0
 BEQ SOMEBITS
 SBTL 'BOOT BLOCK 0 AND 1 CODE'
 REP 50
*
*  BOOT BLOCK 0 CODE
*
 REP 50
*
BOOTZERO EQU * ;WRITTEN TO BLOCK 0
 DFB $01,$38,$B0,$03,$4C,$32,$A1,$86
 DFB $43,$C9,$03,$08,$8A,$29,$70,$4A
 DFB $4A,$4A,$4A,$09,$C0,$85,$49,$A0
 DFB $FF,$84,$48,$28,$C8,$B1,$48,$D0
 DFB $3A,$B0,$0E,$A9,$03,$8D,$00,$08
 DFB $E6,$3D,$A5,$49,$48,$A9,$5B,$48
 DFB $60,$85,$40,$85,$48,$A0,$63,$B1
 DFB $48,$99,$94,$09,$C8,$C0,$EB,$D0
 DFB $F6,$A2,$06,$BC,$1D,$09,$BD,$24
 DFB $09,$99,$F2,$09,$BD,$2B,$09,$9D
 DFB $7F,$0A,$CA,$10,$EE,$A9,$09,$85
 DFB $49,$A9,$86,$A0,$00,$C9,$F9,$B0
 DFB $2F,$85,$48,$84,$60,$84,$4A,$84
 DFB $4C,$84,$4E,$84,$47,$C8,$84,$42
 DFB $C8,$84,$46,$A9,$0C,$85,$61,$85
 DFB $4B,$20,$12,$09,$B0,$68,$E6,$61
 DFB $E6,$61,$E6,$46,$A5,$46,$C9,$06
 DFB $90,$EF,$AD,$00,$0C,$0D,$01,$0C
 DFB $D0,$6D,$A9,$04,$D0,$02,$A5,$4A
 DFB $18,$6D,$23,$0C,$A8,$90,$0D,$E6
 DFB $4B,$A5,$4B,$4A,$B0,$06,$C9,$0A
 DFB $F0,$55,$A0,$04,$84,$4A,$AD,$02
 DFB $09,$29,$0F,$A8,$B1,$4A,$D9,$02
 DFB $09,$D0,$DB,$88,$10,$F6,$29,$F0
 DFB $C9,$20,$D0,$3B,$A0,$10,$B1,$4A
 DFB $C9,$FF,$D0,$33,$C8,$B1,$4A,$85
 DFB $46,$C8,$B1,$4A,$85,$47,$A9,$00
 DFB $85,$4A,$A0,$1E,$84,$4B,$84,$61
 DFB $C8,$84,$4D,$20,$12,$09,$B0,$17
 DFB $E6,$61,$E6,$61,$A4,$4E,$E6,$4E
 DFB $B1,$4A,$85,$46,$B1,$4C,$85,$47
 DFB $11,$4A,$D0,$E7,$4C,$00,$20,$4C
 DFB $3F,$09,$26,$50,$52,$4F,$44,$4F
 DFB $53,$20,$20,$20,$20,$20,$20,$20
 DFB $20,$20,$A5,$60,$85,$44,$A5,$61
 DFB $85,$45,$6C,$48,$00,$08,$1E,$24
 DFB $3F,$45,$47,$76,$F4,$D7,$D1,$B6
 DFB $4B,$B4,$AC,$A6,$2B,$18,$60,$4C
 DFB $BC,$09,$A9,$9F,$48,$A9,$FF,$48
 DFB $A9,$01,$A2,$00,$4C,$79,$F4,$20
 DFB $58,$FC,$A0,$1C,$B9,$50,$09,$99
 DFB $AE,$05,$88,$10,$F7,$4C,$4D,$09
 DFB $AA,$AA,$AA,$A0,$D5,$CE,$C1,$C2
 DFB $CC,$C5,$A0,$D4,$CF,$A0,$CC,$CF
 DFB $C1,$C4,$A0,$D0,$D2,$CF,$C4,$CF
 DFB $D3,$A0,$AA,$AA,$AA,$A5,$53,$29
 DFB $03,$2A,$05,$2B,$AA,$BD,$80,$C0
 DFB $A9,$2C,$A2,$11,$CA,$D0,$FD,$E9
 DFB $01,$D0,$F7,$A6,$2B,$60,$A5,$46
 DFB $29,$07,$C9,$04,$29,$03,$08,$0A
 DFB $28,$2A,$85,$3D,$A5,$47,$4A,$A5
 DFB $46,$6A,$4A,$4A,$85,$41,$0A,$85
 DFB $51,$A5,$45,$85,$27,$A6,$2B,$BD
 DFB $89,$C0,$20,$BC,$09,$E6,$27,$E6
 DFB $3D,$E6,$3D,$B0,$03,$20,$BC,$09
 DFB $BC,$88,$C0,$60,$A5,$40,$0A,$85
 DFB $53,$A9,$00,$85,$54,$A5,$53,$85
 DFB $50,$38,$E5,$51,$F0,$14,$B0,$04
 DFB $E6,$53,$90,$02,$C6,$53,$38,$20
 DFB $6D,$09,$A5,$50,$18,$20,$6F,$09
 DFB $D0,$E3,$A0,$7F,$84,$52,$08,$28
 DFB $38,$C6,$52,$F0,$CE,$18,$08,$88
 DFB $F0,$F5,$BD,$8C,$C0,$10,$FB,$00
 DFB $00,$00,$00,$00,$00,$00,$00,$00
 PAGE
 REP 50
*
*  BOOT BLOCK 1 CODE
*
 REP 50
*
BOOTONE EQU * ;WRITTEN TO BLOCK 1
 DFB $4C,$6E,$A0,$53,$4F,$53,$20,$42
 DFB $4F,$4F,$54,$20,$20,$31,$2E,$31
 DFB $20,$0A,$53,$4F,$53,$2E,$4B,$45
 DFB $52,$4E,$45,$4C,$20,$20,$20,$20
 DFB $20,$53,$4F,$53,$20,$4B,$52,$4E
 DFB $4C,$49,$2F,$4F,$20,$45,$52,$52
 DFB $4F,$52,$08,$00,$46,$49,$4C,$45
 DFB $20,$27,$53,$4F,$53,$2E,$4B,$45
 DFB $52,$4E,$45,$4C,$27,$20,$4E,$4F
 DFB $54,$20,$46,$4F,$55,$4E,$44,$25
 DFB $00,$49,$4E,$56,$41,$4C,$49,$44
 DFB $20,$4B,$45,$52,$4E,$45,$4C,$20
 DFB $46,$49,$4C,$45,$3A,$00,$00,$0C
 DFB $00,$1E,$0E,$1E,$04,$A4,$78,$D8
 DFB $A9,$77,$8D,$DF,$FF,$A2,$FB,$9A
 DFB $2C,$10,$C0,$A9,$40,$8D,$CA,$FF
 DFB $A9,$07,$8D,$EF,$FF,$A2,$00,$CE
 DFB $EF,$FF,$8E,$00,$20,$AD,$00,$20
 DFB $D0,$F5,$A9,$01,$85,$E0,$A9,$00
 DFB $85,$E1,$A9,$00,$85,$85,$A9,$A2
 DFB $85,$86,$20,$BE,$A1,$E6,$E0,$A9
 DFB $00,$85,$E6,$E6,$86,$E6,$86,$E6
 DFB $E6,$20,$BE,$A1,$A0,$02,$B1,$85
 DFB $85,$E0,$C8,$B1,$85,$85,$E1,$D0
 DFB $EA,$A5,$E0,$D0,$E6,$AD,$6C,$A0
 DFB $85,$E2,$AD,$6D,$A0,$85,$E3,$18
 DFB $A5,$E3,$69,$02,$85,$E5,$38,$A5
 DFB $E2,$ED,$23,$A4,$85,$E4,$A5,$E5
 DFB $E9,$00,$85,$E5,$A0,$00,$B1,$E2
 DFB $29,$0F,$CD,$11,$A0,$D0,$21,$A8
 DFB $B1,$E2,$D9,$11,$A0,$D0,$19,$88
 DFB $D0,$F6,$A0,$00,$B1,$E2,$29,$F0
 DFB $C9,$20,$F0,$3E,$C9,$F0,$F0,$08
 DFB $AE,$64,$A0,$A0,$13,$4C,$D4,$A1
 DFB $18,$A5,$E2,$6D,$23,$A4,$85,$E2
 DFB $A5,$E3,$69,$00,$85,$E3,$A5,$E4
 DFB $C5,$E2,$A5,$E5,$E5,$E3,$B0,$BC
 DFB $18,$A5,$E4,$6D,$23,$A4,$85,$E2
 DFB $A5,$E5,$69,$00,$85,$E3,$C6,$E6
 DFB $D0,$95,$AE,$4F,$A0,$A0,$1B,$4C
 DFB $D4,$A1,$A0,$11,$B1,$E2,$85,$E0
 DFB $C8,$B1,$E2,$85,$E1,$AD,$66,$A0
 DFB $85,$85,$AD,$67,$A0,$85,$86,$20
 DFB $BE,$A1,$AD,$68,$A0,$85,$85,$AD
 DFB $69,$A0,$85,$86,$AD,$00,$0C,$85
 DFB $E0,$AD,$00,$0D,$85,$E1,$20,$BE
 DFB $A1,$A2,$07,$BD,$00,$1E,$DD,$21
 DFB $A0,$F0,$08,$AE,$64,$A0,$A0,$13
 DFB $4C,$D4,$A1,$CA,$10,$ED,$A9,$00
 DFB $85,$E7,$E6,$E7,$E6,$86,$E6,$86
 DFB $A6,$E7,$BD,$00,$0C,$85,$E0,$BD
 DFB $00,$0D,$85,$E1,$A5,$E0,$D0,$04
 DFB $A5,$E1,$F0,$06,$20,$BE,$A1,$4C
 DFB $8A,$A1,$18,$AD,$6A,$A0,$6D,$08
 DFB $1E,$85,$E8,$AD,$6B,$A0,$6D,$09
 DFB $1E,$85,$E9,$6C,$E8,$00,$A9,$01
 DFB $85,$87,$A5,$E0,$A6,$E1,$20,$79
 DFB $F4,$B0,$01,$60,$AE,$32,$A0,$A0
 DFB $09,$4C,$D4,$A1,$84,$E7,$38,$A9
 DFB $28,$E5,$E7,$4A,$18,$65,$E7,$A8
 DFB $BD,$29,$A0,$99,$A7,$05,$CA,$88
 DFB $C6,$E7,$D0,$F4,$AD,$40,$C0,$4C
 DFB $EF,$A1,$00,$00,$00,$00,$00,$00
 DFB $00,$00,$00,$00,$00,$00,$00,$00
 DFB $00,$00,$03,$00,$F6,$50,$52,$4F
loctemp dfb 0
locbuff ds $E,0
ZBDEND EQU *
poslo equ >ZBDEND ;extract lowbyte of current pc
ZBDLEN EQU ZBDEND-BUILDDISK ;length of Buildisk, less formatter
 ds $100-poslo ;fill to next page boundry
 sbtl 'Disk II FORMATTER'
 page
FORMATTII equ * ;here is the diskII formatter.
*
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
* *                                                 * *
* * M U S T   B E   O N   P A G E   B O U N D A R Y * *
* *                                                 * *
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
*                                                     *
*  ProDOS DISK ][ Formatter Device Driver             *  
*                                                     *
*  Copyright Apple Computer, Inc., 1982-1984          *
*                                                     *
*  Enter with ProDOS device number in A-register:     *
*         Zero    = bits 0, 1, 2, 3                   *
*         Slot No.= bits 4, 5, 6                      * 
*         Drive 1 = bit 7 off                         * 
*         Drive 2 = bit 7 on                          * 
*                                                     *
*  Error codes returned in A-register:                *
*         $00 : Good completion                       * 
*         $27 : Unable to format                      * 
*         $2B : Write-Protected                       * 
*         $33 : Drive too SLOW                        * 
*         $34 : Drive too FAST                        * 
*         NOTE: Carry flag is set if error occured.   *
*                                                     *
*  Uses zero page locations $D0 thru $DD              *
*                                                     *
* - - - - - - - - - - - - - - - - - - - - - - - - - - *
* Modified 15 December 1983 to disable interrupts     *
* Modified 20 December 1983 to increase tolerance     *
*    of disk speed check                              *
* Modified 30 March 1983 to increase tolerance of     *
*    disk speed                                       *
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
 PHP
 SEI
 JSR *+$38 
 PLP
 CMP #$00
 BNE *+$04
 CLC
 RTS
 CMP #$02
 BNE *+$07
 LDA #$2B
 JMP *+$0F 
 CMP #$01
 BNE *+$07
 LDA #$27
 JMP *+$06 
 CLC
 ADC #$30
 SEC
 RTS
 ASL A
 ASL *+$400
 STA *+$40F
 TXA
 LSR A
 LSR A
 LSR A
 LSR A
 TAY
 LDA *+$406
 JSR *+$193
 LSR *+$3EE 
 RTS
 TAX
 AND #$70
 STA *+$3E6
 TXA
 LDX *+$3E2 
 ROL A
 LDA #$00 
 ROL A 
 BNE *+$08
 LDA $C08A,X
 JMP *+$06
 LDA $C08B,X
 LDA $C089,X
 LDA #$D7
 STA $DA
 LDA #$50
 STA *+$3C8 
 LDA #$00
 JSR *-$3E
 LDA $DA
 BEQ *+$08
 JSR *+$2D2 
 JMP *-$07
 LDA #$01
 STA $D3
 LDA #$AA
 STA $D0
 LDA *+$3AA
 CLC
 ADC #$02
 STA $D4
 LDA #$00
 STA $D1 
 LDA $D1
 LDX *+$39F
 JSR *-$64
 LDX *+$399
 LDA $C08D,X
 LDA $C08E,X
 TAY
 LDA $C08E,X
 LDA $C08C,X
 TYA
 BPL *+$07
 LDA #$02 
 JMP *+$5A 
 JSR *+$2C1
 BCC *+$10
 LDA #$01
 LDY $D4
 CPY *+$374
 BCS *+$04
 LDA #$04
 JMP *+$47 
 LDY $D4
 CPY *+$368
 BCS *+$07
 LDA #$04
 JMP *+$3B
 CPY *+$35F 
 BCC *+$07
 LDA #$03
 JMP *+$31 
 LDA *+$357
 STA *+$357
 DEC *+$354
 BNE *+$07
 LDA #$01
 JMP *+$21
 LDX *+$348
 JSR *+$8C
 BCS *-$10
 LDA $D8
 BNE *-$14 
 LDX *+$33C
 JSR *+$1D
 BCS *-$1C 
 INC $D1
 LDA $D1
 CMP #$23
 BCC *-$73 
 LDA #$00
 PHA
 LDX *+$329
 LDA $C088,X
 LDA #$00
 JSR *-$DF
 PLA
 RTS
 LDY #$20
 DEY
 BEQ *+$5E 
 LDA $C08C,X
 BPL *-$03
 EOR #$D5
 BNE *-$0A
 NOP
 LDA $C08C,X
 BPL *-$03
 CMP #$AA
 BNE *-$0C
 LDY #$56
 LDA $C08C,X
 BPL *-$03
 CMP #$AD
 BNE *-$17 
 LDA #$00
 DEY
 STY $D5
 LDA $C08C,X
 BPL *-$03
 CMP #$96
 BNE *+$32
 LDY $D5
 BNE *-$0E
 STY $D5
 LDA $C08C,X
 BPL *-$03
 CMP #$96
 BNE *+$23
 LDY $D5
 INY
 BNE *-$0E
 LDA $C08C,X
 BPL *-$03
 CMP #$96
 BNE *+$15
 LDA $C08C,X
 BPL *-$03
 CMP #$DE
 BNE *+$0C
 NOP
 LDA $C08C,X
 BPL *-$03
 CMP #$AA
 BEQ *+$5E
 SEC
 RTS
 LDY #$FC
 STY $DC
 INY
 BNE *+$06
 INC $DC
 BEQ *-$0B 
 LDA $C08C,X
 BPL *-$03
 CMP #$D5
 BNE *-$0E
 NOP
 LDA $C08C,X
 BPL *-$03
 CMP #$AA
 BNE *-$0C
 LDY #$03
 LDA $C08C,X
 BPL *-$03
 CMP #$96
 BNE *-$17
 LDA #$00
 STA $DB
 LDA $C08C,X
 BPL *-$03
 ROL A
 STA $DD
 LDA $C08C,X
 BPL *-$03
 AND $DD
 STA $D7,Y
 EOR $DB
 DEY
 BPL *-$17
 TAY
 BNE *-$47
 LDA $C08C,X
 BPL *-$03
 CMP #$DE
 BNE *-$50
 NOP
 LDA $C08C,X
 BPL *-$03
 CMP #$AA
 BNE *-$5A
 CLC
 RTS
 STX *+$271
 STA *+$26D
 CMP *+$258
 BEQ *+$5E
 LDA #$00 
 STA *+$265
 LDA *+$24E
 STA *+$260
 SEC
 SBC *+$259
 BEQ *+$39
 BCS *+$09
 EOR #$FF
 INC *+$23E
 BCC *+$07
 ADC #$FE
 DEC *+$237 
 CMP *+$248
 BCC *+$05
 LDA *+$243
 CMP #$0C
 BCS *+$03
 TAY
 SEC
 JSR *+$1F
 LDA *+$14A,Y
 JSR *+$136
 LDA *+$232
 CLC
 JSR *+$15
 LDA *+$149,Y
 JSR *+$129
 INC *+$224
 BNE *-$41
 JSR *+$121 
 CLC
 LDA *+$207 
 AND #$03
 ROL A
 ORA *+$214
 TAX
 LDA $C080,X
 LDX *+$20D
 RTS
 JSR *+$1E0 
 LDA $C08D,X
 LDA $C08E,X
 LDA #$FF
 STA $C08F,X 
 CMP $C08C,X
 PHA
 PLA
 NOP
 LDY #$04
 PHA
 PLA
 JSR *+$5F
 DEY
 BNE *-$06
 LDA #$D5
 JSR *+$56
 LDA #$AA
 JSR *+$51
 LDA #$AD
 JSR *+$4C
 LDY #$56
 NOP
 NOP
 NOP
 BNE *+$05
 JSR *+$1AC
 NOP
 NOP
 LDA #$96
 STA $C08D,X
 CMP $C08C,X
 DEY
 BNE *-$0E
 BIT $00
 NOP
 JSR *+$199
 LDA #$96
 STA $C08D,X
 CMP $C08C,X
 LDA #$96
 NOP
 INY
 BNE *-$0F
 JSR *+$1E
 LDA #$DE
 JSR *+$19
 LDA #$AA
 JSR *+$14
 LDA #$EB
 JSR *+$0F
 LDA #$FF
 JSR *+$0A
 LDA $C08E,X
 LDA $C08C,X
 RTS
 NOP
 PHA
 PLA
 STA $C08D,X
 CMP $C08C,X
 RTS
 SEC
 LDA $C08D,X
 LDA $C08E,X
 BMI *+$60
 LDA #$FF
 STA $C08F,X
 CMP $C08C,X
 PHA
 PLA
 JSR *+$5A
 JSR *+$57
 STA $C08D,X
 CMP $C08C,X
 NOP
 DEY
 BNE *-$0E
 LDA #$D5
 JSR *+$5A
 LDA #$AA
 JSR *+$55
 LDA #$96
 JSR *+$50
 LDA $D3
 JSR *+$3A
 LDA $D1
 JSR *+$35
 LDA $D2
 JSR *+$30
 LDA $D3
 EOR $D1
 EOR $D2
 PHA
 LSR A
 ORA $D0
 STA $C08D,X
 LDA $C08C,X
 PLA
 ORA #$AA
 JSR *+$2A
 LDA #$DE
 JSR *+$26
 LDA #$AA
 JSR *+$21
 LDA #$EB
 JSR *+$1C
 CLC
 LDA $C08E,X
 LDA $C08C,X
 RTS
 PHA
 LSR A
 ORA $D0
 STA $C08D,X
 CMP $C08C,X
 PLA
 NOP
 NOP
 NOP
 ORA #$AA
 NOP
 NOP
 PHA
 PLA
 STA $C08D,X
 CMP $C08C,X
 RTS
 BRK
 BRK
 BRK
 LDX #$11
 DEX
 BNE *-$01 
 INC $D9
 BNE *+$04
 INC $DA
 SEC
 SBC #$01
 BNE *-$0E
 RTS
 DFB $01,$30,$28
 DFB $24,$20,$1E
 DFB $1D,$1C,$1C
 DFB $1C,$1C,$1C
 DFB $70,$2C,$26
 DFB $22,$1F,$1E
 DFB $1D,$1C,$1C
 DFB $1C,$1C,$1C
 LDA *+$BE
 STA $D6
 LDY #$80
 LDA #$00
 STA $D2
 JMP *+$05
 LDY $D4
 LDX *+$B0
 JSR *-$C8
 BCC *+$05
 JMP *+$93 
 LDX *+$A5 
 JSR *-$153 
 INC $D2
 LDA $D2
 CMP #$10
 BCC *-$19
 LDY #$0F
 STY $D2
 LDA *+$92
 STA *+$92
 STA *+$90,Y
 DEY
 BPL *-$04
 LDA $D4
 SEC
 SBC #$05
 TAY
 JSR *+$6C
 JSR *+$69
 PHA
 PLA
 NOP
 NOP
 DEY
 BNE *-$0B
 LDX *+$74
 JSR *-$248
 BCS *+$3E
 LDA $D8
 BEQ *+$15
 DEC $D4
 LDA $D4
 CMP *+$60
 BCS *+$31
 SEC
 RTS
 LDX *+$5D
 JSR *-$25F
 BCS *+$1C
 LDX *+$55
 JSR *-$2CA
 BCS *+$14
 LDY $D8
 LDA *+$4E,Y
 BMI *+$0D
 LDA #$FF 
 STA *+$47,Y
 DEC $D2
 BPL *-$1E
 CLC
 RTS
 DEC *+$3D
 BNE *-$25
 DEC $D6
 BNE *+$04
 SEC
 RTS
 LDA *+$2F 
 ASL A
 STA *+$2E
 LDX *+$29
 JSR *-$293
 BCS *+$08
 LDA $D8
 CMP #$0F 
 BEQ *+$09
 DEC *+$1D
 BNE *-$11
 SEC
 RTS
 LDX #$D6
 JSR *-$03
 JSR *-$06
 BIT $00
 DEX
 BNE *-$09
 JMP *-$B4
 DFB $0E,$1B,$03 
 DFB $10,$00,$00
 DFB $00,$00,$00
 DFB $00,$00,$00
 DFB $00,$00,$00
 DFB $00,$00,$00
 DFB $00,$00,$00
 DFB $00,$00,$00
 DFB $00,$00,$00 
Zend equ *
Ztotal equ Zend-Builddisk
