// Globals (Public)
	extern uint16_t g_nVideoClockVert;
	extern uint16_t g_nVideoClockHorz;
	extern uint32_t g_nChromaSize;

// Prototypes (Public) ________________________________________________
	extern void     NTSC_SetVideoMode( uint32_t uVideoModeFlags, bool bDelay=false );
	extern void     NTSC_SetVideoStyle();
	extern void     NTSC_SetVideoTextMode( int cols );
	extern uint32_t*NTSC_VideoGetChromaTable( bool bHueTypeMonochrome, bool bMonitorTypeColorTV );
	extern void     NTSC_VideoClockResync( const DWORD dwCyclesThisFrame );
	extern uint16_t NTSC_VideoGetScannerAddress( const ULONG uExecutedCycles );
	extern uint16_t NTSC_VideoGetScannerAddressForDebugger(void);
	extern void     NTSC_VideoInit( uint8_t *pFramebuffer );
	extern void     NTSC_VideoReinitialize( DWORD cyclesThisFrame, bool bInitVideoScannerAddress );
	extern void     NTSC_VideoInitAppleType();
	extern void     NTSC_VideoInitChroma();
	extern void     NTSC_VideoUpdateCycles( UINT cycles6502 );
	extern void     NTSC_VideoRedrawWholeScreen( void );

	enum VideoRefreshRate_e;
	void NTSC_SetRefreshRate(VideoRefreshRate_e rate);
	UINT NTSC_GetCyclesPerFrame(void);
	UINT NTSC_GetCyclesPerLine(void);
	UINT NTSC_GetVideoLines(void);
	bool NTSC_IsVisible(void);
