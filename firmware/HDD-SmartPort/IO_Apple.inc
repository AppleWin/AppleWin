;--------------------------------------
; All ][
;--------------------------------------
IO_KBD			 = $C000				; R
IO_KBDSTROBE	 = $C010				; W

IO_CLRPAGE2		 = $C054				; W
IO_SETPAGE2		 = $C055				; W

IO_RROMBNK2		 = $C082				; R
;--------------------------------------
; IIe Specific
;--------------------------------------
IO_IIE_CLR80STORE = $C000				; W
IO_IIE_SET80STORE = $C001				; W
IO_IIE_CLR80DISP  = $C00C				; W
IO_IIE_SET80DISP  = $C00D				; W
IO_IIE_CLRALTCHAR = $C00E				; W
IO_IIE_SETALTCHAR = $C00F				; W

IO_IIE_RD80STORE  = $C018				; R
IO_IIE_RDALTCHAR  = $C01E				; R
IO_IIE_RD80DISP	  = $C01F				; R

IO_IIE_OPENAPPLE  = $C061				; R	
IO_IIE_SOLIDAPPLE = $C062				; R
;--------------------------------------
; IIgs Specific
;--------------------------------------
IO_IIGS_DMAREG	 = $00C037				; R,W
;--------------------------------------
