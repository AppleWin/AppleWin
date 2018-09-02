// Globals (Public)
	extern uint16_t g_nVideoClockVert;
	extern uint16_t g_nVideoClockHorz;
	extern uint32_t g_nChromaSize;

// Prototypes (Public) ________________________________________________
	extern void     NTSC_SetVideoMode( uint32_t uVideoModeFlags );
	extern void     NTSC_SetVideoStyle();
	extern void     NTSC_SetVideoTextMode( int cols );
	extern uint32_t*NTSC_VideoGetChromaTable( bool bHueTypeMonochrome, bool bMonitorTypeColorTV );
	extern void     NTSC_VideoClockResync( const DWORD dwCyclesThisFrame );
	extern uint16_t NTSC_VideoGetScannerAddress( const ULONG uExecutedCycles );
	extern void     NTSC_VideoInit( uint8_t *pFramebuffer );
	extern void     NTSC_VideoReinitialize( DWORD cyclesThisFrame );
	extern void     NTSC_VideoInitAppleType();
	extern void     NTSC_VideoInitChroma();
	extern void     NTSC_VideoUpdateCycles( long cycles6502 );
	extern void     NTSC_VideoRedrawWholeScreen( void );
	extern UINT     NTSC_GetFrameBufferBorderlessWidth( void );
