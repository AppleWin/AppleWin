; Disk P5 PROM 
SlotX16         =   $2B ; i.e. $60 = Slot 6. NB. Booting a hard disk sets this to zero.

; IO Switches
IO_STORE40      = $C000 ; W
IO_KEYBOARD     = $C000 ;R
IO_KEYSTROBE    = $C010
IO_ALTZPOFF     = $C008
IO_CLR80VID     = $C00C
IO_CLRALTCHAR   = $C00E ; Turn off Mouse Text
IO_DHIRESOFF    = $C05F
IO_ROMIN        = $C081

; F800 ROM Entry Points
E0_BASICWARM    = $E003
F8_RESET        = $FA69 ; CTRL-RESET
F8_MONITOR      = $FF69 ; CALL-151
F8_INIT         = $FB2F ;
F8_HOME         = $FC58 ; Clear 40x24 text screen
F8_SETNORM      = $FE84 ;
F8_SETVID       = $FE93 ; PR#0
F8_SETKBD       = $FE89 ; IN#0
COUT            = $FDED

            ORG $800
Boot        DB  $01             ;
            LDX SlotX16         ;
            TXA                 ;
            JSR InitSlot        ;
Init        STA IO_STORE40      ;
            STA IO_CLR80VID     ;
            STA IO_CLRALTCHAR   ;
            STA IO_DHIRESOFF    ;
            STA IO_ROMIN        ;
            JSR F8_SETKBD       ;
            JSR F8_SETVID       ;
            JSR F8_INIT         ;
            JSR F8_SETNORM      ;
            JSR F8_HOME         ;
            LDX #0              ;
Print       LDA Message,X       ;
            BEQ WaitKey         ;
            JSR COUT            ;
            INX                 ;
            BNE Print           ;
WaitKey     LDA IO_KEYBOARD     ;
            BPL WaitKey         ;
            STA IO_KEYSTROBE    ;
            CMP #"0"            ; '0' ... '9'
            BCC BootSlot        ;
            CMP #"9"+1          ;
            BCS BootSlot        ;
            AND #$0F            ;
            BNE TestMonitor     ; '0' BASIC
            JMP E0_BASICWARM    ;
TestMonitor CMP #$08            ; '8' MONITOR
            BNE TestReset       ;
            JMP F8_MONITOR      ;
TestReset   CMP #$09            ; '9' RESET
            BNE HaveSlot        ;
            JMP F8_RESET        ;
HaveSlot    JSR SetBootSlot     ;
BootSlot    JMP $C700           ; *** SELF-MODIFIED ***
InitSlot    BNE BootSlotX16     ;

            LDA #$70            ;
BootSlotX16 LSR                 ;
            LSR                 ;
            LSR                 ;
            LSR                 ;
SetBootSlot ORA #$C0            ; Firmware $Cx00
            STA BootSlot+2      ; *** SELF-MODIFIES! ***
            RTS
;                0        1         2         3         4
;                1234567890123456789012345678901234567890
Message     ASC "      THIS IS AN EMPTY "
Type        ASC                         "DATA DISK.",$8D
            ASC $8D
            ASC " "," ",'0'," BASIC     ($E003)",$8D
            ASC '1',"-",'7'," BOOT SLOT ($CX00)",$8D
            ASC " "," ",'8'," MONITOR   ($FF69)",$8D
            ASC " "," ",'9'," RESET     ($FA62)",$8D
            ASC 'A','N','Y'," REBOOT LAST SLOT" ,$00
            DS $900 - 3 - *,0   ; reserve 3 bytes: 1 byte offset of "DATA" and 2 byte version string
            ASC "V2"
            DB  <Type           ; byte offset of "DATA" 
