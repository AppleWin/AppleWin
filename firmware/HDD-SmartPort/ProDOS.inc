;--------------------------------------
; ProDOS defines
;--------------------------------------
PRODOS_MAXPATH	 = 64 
;--------------------------------------
; Error codes
;--------------------------------------
E_BADCALL		 = $01					; Bad Call Number
E_BADCNT		 = $04					; Bad Parameter Count
E_BUSERR		 = $06					; Communications Error
E_BADCTL		 = $21					; Invalid Status Code

E_IO		 	 = $27					; I/O Error
E_NODEV			 = $28					; No Device Connected
E_WRTPROT		 = $2B					; Write Protected
E_BADBLOCK		 = $2D					; Invalid Block Number
E_DSKSWIT		 = $2E					; Disk Switched
E_OFFLINE		 = $2F					; Device Offline
;--------------------------------------
