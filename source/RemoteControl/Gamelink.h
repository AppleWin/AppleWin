#ifndef __GAMELINK_H___
#define __GAMELINK_H___

#include <Windows.h>

//------------------------------------------------------------------------------
// Namespace Declaration
//------------------------------------------------------------------------------

namespace GameLink
{

	//--------------------------------------------------------------------------
	// Global Declarations
	//--------------------------------------------------------------------------

#pragma pack( push, 1 )

	//
	// sSharedMMapFrame_R1
	//
	// Server -> Client Frame. 32-bit RGBA up to MAX_WIDTH x MAX_HEIGHT
	//
	struct sSharedMMapFrame_R1
	{
		UINT16 seq;
		UINT16 width;
		UINT16 height;

		UINT8 image_fmt; // 0 = no frame; 1 = 32-bit 0xAARRGGBB
		UINT8 reserved0;

		UINT16 par_x; // pixel aspect ratio
		UINT16 par_y;

		enum { MAX_WIDTH = 1280 };
		enum { MAX_HEIGHT = 1024 };

		enum { MAX_PAYLOAD = MAX_WIDTH * MAX_HEIGHT * 4 };
		UINT8 buffer[ MAX_PAYLOAD ];
	};

	//
	// sSharedMMapInput_R2
	//
	// Client -> Server Input Data
	//

	struct sSharedMMapInput_R2
	{
		float mouse_dx;
		float mouse_dy;
		UINT8 ready;
		UINT8 mouse_btn;
		UINT keyb_state[ 8 ];
	};

	//
	// sSharedMMapPeek_R2
	//
	// Memory reading interface.
	//
	struct sSharedMMapPeek_R2
	{
		enum { PEEK_LIMIT = 16 * 1024 };

		UINT addr_count;
		UINT addr[ PEEK_LIMIT ];
		UINT8 data[ PEEK_LIMIT ];
	};

	//
	// sSharedMMapBuffer_R1
	//
	// General buffer (64Kb)
	//
	struct sSharedMMapBuffer_R1
	{
		enum { BUFFER_SIZE = ( 64 * 1024 ) };

		UINT16 payload;
		UINT8 data[ BUFFER_SIZE ];
	};

	//
	// sSharedMMapAudio_R1
	//
	// Audio control interface.
	//
	struct sSharedMMapAudio_R1
	{
		UINT8 master_vol_l;
		UINT8 master_vol_r;
	};

	//
	// sSharedMemoryMap_R4
	//
	// Memory Map (top-level object)
	//
	struct sSharedMemoryMap_R4
	{
		enum {
			FLAG_WANT_KEYB			= 1 << 0,
			FLAG_WANT_MOUSE			= 1 << 1,
			FLAG_NO_FRAME			= 1 << 2,
			FLAG_PAUSED				= 1 << 3,
		};

		enum {
			SYSTEM_MAXLEN			= 64
		};

		enum {
			PROGRAM_MAXLEN			= 260
		};

		UINT8 version; // = PROTOCOL_VER
		UINT8 flags;
		char system[SYSTEM_MAXLEN] = {}; // System name.
		char program[PROGRAM_MAXLEN] = {}; // Program name. Zero terminated.
		UINT program_hash[4] = { 0,0,0,0 }; // Program code hash (256-bits)

		sSharedMMapFrame_R1 frame;
		sSharedMMapInput_R2 input;
		sSharedMMapPeek_R2 peek;
		sSharedMMapBuffer_R1 buf_recv; // a message to us.
		sSharedMMapBuffer_R1 buf_tohost;
		sSharedMMapAudio_R1 audio;

		// added for protocol v4
		UINT ram_size;
	};

#pragma pack( pop )


	//--------------------------------------------------------------------------
	// Global Functions
	//--------------------------------------------------------------------------

	extern bool GetGameLinkEnabled(void);
	extern void SetGameLinkEnabled(const bool bEnabled);
	extern bool GetTrackOnlyEnabled(void);
	extern void SetTrackOnlyEnabled(const bool bEnabled);

	extern int Init( const bool trackonly_mode );
	
	extern UINT8* AllocRAM( const UINT size );

	extern void Term();

	extern void SetProgramInfo(const std::string name, UINT i1, UINT i2, UINT i3, UINT i4);

	extern int In( sSharedMMapInput_R2* p_input,
				   sSharedMMapAudio_R1* p_audio );

	extern void Out(const UINT8* p_sysmem);

	extern void Out( const UINT16 frame_width,
					 const UINT16 frame_height,
					 const double source_ratio,
					 const bool need_mouse,
					 const UINT8* p_frame,
					 const UINT8* p_sysmem );

	extern void ExecTerminal(sSharedMMapBuffer_R1* p_inbuf,
		sSharedMMapBuffer_R1* p_outbuf,
		sSharedMMapBuffer_R1* p_mechbuf);

	extern void ExecTerminalMech(sSharedMMapBuffer_R1* p_mechbuf);

	extern void InitTerminal();

}; // namespace GameLink

//==============================================================================

#endif // __GAMELINK_HDR__
