// Constants
	const int VIDEO_SCANNER_6502_CYCLES = 17030;

// Globals (Public)
	extern uint16_t g_nVideoClockVert;
	extern uint16_t g_nVideoClockHorz;

// Prototypes (Public) ________________________________________________
	extern void    NTSC_SetVideoTextMode( int cols );
	extern void    NTSC_SetVideoMode( int flags );
	extern void    NTSC_SetVideoStyle();

	extern uint8_t NTSC_VideoGetByte(unsigned long);
	extern void    NTSC_VideoInit( uint8_t *pFramebuffer );
	extern void    NTSC_VideoInitAppleType ();
	extern bool    NTSC_VideoIsVbl();
	extern void    NTSC_VideoUpdateCycles( long cycles );

