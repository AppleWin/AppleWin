// Game Link
//
// Game Link is an API created by DWD for Grid Cartographer, to link his map-making software
// to emulators like DosBox and FS-UAE. There is a version of DosBox called dosbox-gridc that implements this API,
// as well as multiple other emulators.
// Game Link's core premise is to link the emulator and 3rd party programs via shared memory.
// There are 4 main pieces:
// 	- A map that contains input from the 3rd party into the emulator, containing:
//		- a structure with audio volume information
//		- a structure that passes the current state of the keyboard and the delta mouse
// 	- A map that contains output from the emulator, containing the emulated hardware RAM and frame information
// 	- Also included in the output is information about the active emulated program and its unique signature
// 	- A special terminal-style interface to pass to the emulator commands that are not keystrokes (reset, quit, pause, etc...)
//
// Applewin's code:
//	- allocates main and aux mem as shared memory
//	- creates a running program signature with help from Signatures.cpp
//	- calls Gamelink::In() and Gamelink::Out() to grab commands/keystrokes and send video & ram respectively

#include <Windows.h>
#include "Common.h"
#include "StdAfx.h"
#include "Log.h"
#include "Gamelink.h"
#include "Applewin.h"
#include "Frame.h"

//------------------------------------------------------------------------------
// Local Definitions
//------------------------------------------------------------------------------

#define SYSTEM_NAME		"AppleWin"
#define PROTOCOL_VER		4
#define GAMELINK_MUTEX_NAME		"DWD_GAMELINK_MUTEX_R4"
#define GAMELINK_MMAP_NAME		"DWD_GAMELINK_MMAP_R4"

using namespace GameLink;

//------------------------------------------------------------------------------
// Local Data
//------------------------------------------------------------------------------

static HANDLE g_mutex_handle;
static HANDLE g_mmap_handle;

static bool g_bEnableGamelink;
static bool g_bEnableTrackOnly;

static UINT g_membase_size;

static GameLink::sSharedMemoryMap_R4* g_p_shared_memory;
static GameLink::sSharedMMapBuffer_R1* g_p_outbuf;

#define MEMORY_MAP_CORE_SIZE sizeof( GameLink::sSharedMemoryMap_R4 )


//------------------------------------------------------------------------------
// Local Functions
//------------------------------------------------------------------------------

static void shared_memory_init()
{
	// Initialise

	g_p_shared_memory->version = PROTOCOL_VER;
	g_p_shared_memory->flags = 0;

	memset( g_p_shared_memory->system, 0, sizeof( g_p_shared_memory->system ) );
	strcpy( g_p_shared_memory->system, SYSTEM_NAME );
	memset( g_p_shared_memory->program, 0, sizeof( g_p_shared_memory->program ) );

	g_p_shared_memory->program_hash[0] = 0;
	g_p_shared_memory->program_hash[1] = 0;
	g_p_shared_memory->program_hash[2] = 0;
	g_p_shared_memory->program_hash[3] = 0;

	// reset input
	g_p_shared_memory->input.mouse_dx = 0;
	g_p_shared_memory->input.mouse_dy = 0;

	g_p_shared_memory->input.mouse_btn = 0;
	for ( int i = 0; i < 8; ++i ) {
		g_p_shared_memory->input.keyb_state[ i ] = 0;
	}

	// reset peek interface
	g_p_shared_memory->peek.addr_count = 0;
	memset( g_p_shared_memory->peek.addr, 0, GameLink::sSharedMMapPeek_R2::PEEK_LIMIT * sizeof( UINT ) );
	memset( g_p_shared_memory->peek.data, 0, GameLink::sSharedMMapPeek_R2::PEEK_LIMIT * sizeof( UINT8 ) );

	// blank frame
	g_p_shared_memory->frame.seq = 0;
	g_p_shared_memory->frame.image_fmt = 0; // = no frame
	g_p_shared_memory->frame.width = 0;
	g_p_shared_memory->frame.height = 0;

	g_p_shared_memory->frame.par_x = 1;
	g_p_shared_memory->frame.par_y = 1;
	memset( g_p_shared_memory->frame.buffer, 0, GameLink::sSharedMMapFrame_R1::MAX_PAYLOAD );

	// audio: 100%
	g_p_shared_memory->audio.master_vol_l = 100;
	g_p_shared_memory->audio.master_vol_r = 100;

	// RAM
	g_p_shared_memory->ram_size = g_membase_size;
}

//
// create_mutex
//
// Create a globally unique mutex.
//
// \returns 1 if we made one, 0 if it failed or the mutex existed already (also a failure).
//
static int create_mutex( const char* p_name )
{

	// The mutex is already open?
	g_mutex_handle = OpenMutexA( SYNCHRONIZE, FALSE, p_name );
	if ( g_mutex_handle != 0 ) {
		// .. didn't fail - so must already exist.
		CloseHandle( g_mutex_handle );
		g_mutex_handle = 0;
		return 0;
	}

	// Actually create it.
	g_mutex_handle = CreateMutexA( NULL, FALSE, p_name );
	if ( g_mutex_handle ) {
		return 1;
	}

	return 0;
}

//
// destroy_mutex
//
// Tidy up the mutex.
//
static void destroy_mutex( const char* p_name )
{
	(void)(p_name);

	if ( g_mutex_handle )
	{
		CloseHandle( g_mutex_handle );
		g_mutex_handle = NULL;
	}
}

//
// create_shared_memory
//
// Create a shared memory area.
//
// \returns 1 if we made one, 0 if it failed.
//
static int create_shared_memory()
{
	const int memory_map_size = MEMORY_MAP_CORE_SIZE + g_membase_size;

	g_mmap_handle = CreateFileMappingA( INVALID_HANDLE_VALUE, NULL,
			PAGE_READWRITE, 0, memory_map_size,	GAMELINK_MMAP_NAME );

	if ( g_mmap_handle )
	{
		g_p_shared_memory = reinterpret_cast< GameLink::sSharedMemoryMap_R4* >(
			MapViewOfFile( g_mmap_handle, FILE_MAP_ALL_ACCESS, 0, 0, memory_map_size )
			);

		if ( g_p_shared_memory )
		{
			return 1; // Success!
		}
		else
		{
			// tidy up file mapping.
			CloseHandle( g_mmap_handle );
			g_mmap_handle = NULL;
		}
	}

	// Failure
	return 0;
}

//
// destroy_shared_memory
//
// Destroy the shared memory area.
//
static void destroy_shared_memory()
{
	const int memory_map_size = MEMORY_MAP_CORE_SIZE + g_membase_size;

	if ( g_p_shared_memory )
	{
		UnmapViewOfFile( g_p_shared_memory );
		g_p_shared_memory = NULL;
	}

	if ( g_mmap_handle )
	{
		CloseHandle( g_mmap_handle );
		g_mmap_handle = NULL;
	}
}

//
// proc_mech
//
// Process a mechanical command - encoded form for computer-computer communication. Minimal feedback.
//
static void proc_mech(GameLink::sSharedMMapBuffer_R1* cmd, UINT16 payload)
{
	// Ignore NULL commands.
	if (payload <= 1 || payload > 128)
		return;

	cmd->payload = 0;
	char* com = (char*)(cmd->data);
	com[payload] = 0;

	//	printf( "command = %s; payload = %d\n", com, payload );

		//
		// Decode

	if (strcmp(com, ":reset") == 0)
	{
		ProcessButtonClick(BTN_RUN, false);
	}
	else if (strcmp(com, ":pause") == 0)
	{
		LPARAM lparam = 1;
		lparam = lparam | (LPARAM)(0x45 << 16);				// scancode for PAUSE
		PostMessageW(g_hFrameWindow, WM_KEYDOWN, VK_PAUSE, lparam);
		lparam = lparam | (LPARAM)(1 << 30);				// previous key state
		lparam = lparam | (LPARAM)(1 << 31);				// transition state (1 for keyup)
		PostMessageW(g_hFrameWindow, WM_KEYUP, VK_PAUSE, 0);
	}
	else if (strcmp(com, ":shutdown") == 0)
	{
		PostMessageW(g_hFrameWindow, WM_DESTROY, 0, 0);
	}
}

//==============================================================================

//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------
bool GameLink::GetGameLinkEnabled(void)
{
	return g_bEnableGamelink;
}

void GameLink::SetGameLinkEnabled(const bool bEnabled)
{
	g_bEnableGamelink = bEnabled;
}

bool GameLink::GetTrackOnlyEnabled(void)
{
	return g_bEnableTrackOnly;
}

void GameLink::SetTrackOnlyEnabled(const bool bEnabled)
{
	g_bEnableTrackOnly = bEnabled;
}


//------------------------------------------------------------------------------
// GameLink::Init
//------------------------------------------------------------------------------
int GameLink::Init( const bool trackonly_mode )
{
	int iresult;

	// Already initialised?
	if ( g_mutex_handle )
	{
		LogFileOutput("%s\n", "GAMELINK: Ignoring re-initialisation." );

		// success
		return 1;
	}

	// Store the mode we're in.
	g_bEnableTrackOnly = trackonly_mode;

	// Create a fresh mutex.
	iresult = create_mutex( GAMELINK_MUTEX_NAME );
	if ( iresult != 1 )
	{
		// failed.
		return 0;
	}

	return 1;
}

//------------------------------------------------------------------------------
// GameLink::AllocRAM
//------------------------------------------------------------------------------
UINT8* GameLink::AllocRAM( const UINT size )
{
	int iresult;

	g_membase_size = size;

	// Create a shared memory area.
	iresult = create_shared_memory();
	if ( iresult != 1 )
	{
		destroy_mutex( GAMELINK_MUTEX_NAME );
		// failed.
		return 0;
	}

	// Initialise
	shared_memory_init();

	GameLink::InitTerminal();

	const int memory_map_size = MEMORY_MAP_CORE_SIZE + g_membase_size;
	LogFileOutput("GAMELINK: Initialised. Allocated %d MB of shared memory.\n", (memory_map_size + (1024*1024) - 1) / (1024*1024) );

	UINT8* membase = ((UINT8*)g_p_shared_memory) + MEMORY_MAP_CORE_SIZE;

	// Return RAM base pointer.
	return membase;
}

//------------------------------------------------------------------------------
// GameLink::Term
//------------------------------------------------------------------------------
void GameLink::Term()
{
	// SEND ABORT CODE TO CLIENT (don't care if it fails)
	if ( g_p_shared_memory )
		g_p_shared_memory->version = 0;

	destroy_shared_memory();

	destroy_mutex( GAMELINK_MUTEX_NAME );

	g_membase_size = 0;
}


//------------------------------------------------------------------------------
// GameLink::SetProgramInfo
//------------------------------------------------------------------------------
//
// Grid Cartographer uses an array of 4 longs to determine the unique signature
// of a program. Here we convert the Applewin signature of the form "page-crc"
// where the page is a decimal and the crc is a hex, into 2 longs for use by
// Grid Cartographer

void GameLink::SetProgramInfo(const std::string name, UINT i1, UINT i2, UINT i3, UINT i4)
{
	if (g_p_shared_memory)
	{
		std::string szTmp = name.substr(0, sizeof(g_p_shared_memory->program));
		strcpy(g_p_shared_memory->program, szTmp.c_str());
		g_p_shared_memory->program_hash[0] = i1;
		g_p_shared_memory->program_hash[1] = i2;
		g_p_shared_memory->program_hash[2] = i3;
		g_p_shared_memory->program_hash[3] = i4;
	}
}

//------------------------------------------------------------------------------
// GameLink::In
//------------------------------------------------------------------------------
//
// Incoming information from the external program via the Gamelink API
// Copy it to g_gamelink and let Applewin know we're ready

int GameLink::In( GameLink::sSharedMMapInput_R2* p_input,
				  GameLink::sSharedMMapAudio_R1* p_audio )
{
	int ready = 0;

	if ( g_p_shared_memory )
	{
		if (g_bEnableTrackOnly)
		{
			// No input.
			memset( p_input, 0, sizeof( sSharedMMapInput_R2 ) );
		}
		else
		{
			if ( g_p_shared_memory->input.ready )
			{
				// Copy client input out of shared memory
				memcpy( p_input, &( g_p_shared_memory->input ), sizeof( sSharedMMapInput_R2 ) );

				// Clear remote delta, prevent counting more than once.
				g_p_shared_memory->input.mouse_dx = 0;
				g_p_shared_memory->input.mouse_dy = 0;

				g_p_shared_memory->input.ready = 0;

				ready = 1; // Got some input
			}

			if ( g_p_shared_memory->audio.master_vol_l <= 100 )
				p_audio->master_vol_l = g_p_shared_memory->audio.master_vol_l;
			if ( g_p_shared_memory->audio.master_vol_r <= 100 )
				p_audio->master_vol_r = g_p_shared_memory->audio.master_vol_r;
		}
	}

	return ready;
}

//------------------------------------------------------------------------------
// GameLink::Out
//------------------------------------------------------------------------------
//
// Outgoing information to any programs conforming with the Gamelink API
// This must be triggered for every frame if we want correct video output
//
// Version only with memory, used for out-of-band commands
void GameLink::Out(const UINT8* p_sysmem)
{
	Out(0, 0, 1, false, NULL, p_sysmem);
}

// Full version with video out
void GameLink::Out( const UINT16 frame_width,
					const UINT16 frame_height,
					const double source_ratio,
					const bool want_mouse,
					const UINT8* p_frame,
					const UINT8* p_sysmem )
{
	// Not initialised (or disabled) ?
	if ( g_p_shared_memory == NULL ) {
		return; // <=== EARLY OUT
	}

	// Create integer ratio
	UINT16 par_x, par_y;
	if ( source_ratio >= 1.0 )
	{
		par_x = 4096;
		par_y = static_cast< UINT16 >( source_ratio * 4096.0 );
	}
	else
	{
		par_x = static_cast< UINT16 >( 4096.0 / source_ratio );
		par_y = 4096;
	}

	// Build flags
	UINT8 flags;

	if (g_bEnableTrackOnly)
	{
		// Tracking Only - Emulator handles video/input as usual.
		flags = sSharedMemoryMap_R4::FLAG_NO_FRAME;
	}
	else
	{
		// External Input Mode
		flags = sSharedMemoryMap_R4::FLAG_WANT_KEYB;
		if ( want_mouse )
			flags |= sSharedMemoryMap_R4::FLAG_WANT_MOUSE;
	}

	// Paused?
	if (g_nAppMode == MODE_PAUSED)
		flags |= sSharedMemoryMap_R4::FLAG_PAUSED;


	//
	// Send data?

	// Message buffer
	sSharedMMapBuffer_R1 proc_mech_buffer;
	proc_mech_buffer.payload = 0;

	DWORD mutex_result;
	mutex_result = WaitForSingleObject( g_mutex_handle, INFINITE );
	if ( mutex_result == WAIT_OBJECT_0 )

	{

		{ // ========================

			// Set version
			g_p_shared_memory->version = PROTOCOL_VER;

			// Store flags
			g_p_shared_memory->flags = flags;

			if ((g_bEnableTrackOnly == false) && p_frame)
			{
				// Update the frame sequence
				++g_p_shared_memory->frame.seq;

				// Copy frame properties
				g_p_shared_memory->frame.image_fmt = 1; // = 32-bit RGBA
				g_p_shared_memory->frame.width = frame_width;
				g_p_shared_memory->frame.height = frame_height;
				g_p_shared_memory->frame.par_x = par_x;
				g_p_shared_memory->frame.par_y = par_y;

				// Frame Buffer
				UINT payload;
				payload = frame_width * frame_height * 4;
				if ( frame_width <= sSharedMMapFrame_R1::MAX_WIDTH && frame_height <= sSharedMMapFrame_R1::MAX_HEIGHT )
				{
					memcpy( g_p_shared_memory->frame.buffer, p_frame, payload );
				}
			}

			// Peek for special requested memory items
			for ( UINT pindex = 0;
					pindex < g_p_shared_memory->peek.addr_count &&
					pindex < sSharedMMapPeek_R2::PEEK_LIMIT;
				++pindex )
			{
				// read address
				UINT address;
				address = g_p_shared_memory->peek.addr[ pindex ];

				UINT8 data;
				// valid?
				if ( address < g_membase_size )
				{
					data = p_sysmem[ address ]; // read data
				}
				else
				{
					data = 0; // <-- safe
				}

				// write data
				g_p_shared_memory->peek.data[ pindex ] = data;
			}

			// Message Processing.
			ExecTerminal( &(g_p_shared_memory->buf_recv),
						  &(g_p_shared_memory->buf_tohost),
						  &(proc_mech_buffer) );

		} // ========================

		ReleaseMutex( g_mutex_handle );

		// Mechanical Message Processing, out of mutex.
		if ( proc_mech_buffer.payload )
			ExecTerminalMech( &proc_mech_buffer );
	}

}

void GameLink::InitTerminal()
{
	g_p_outbuf = 0;
}

void GameLink::ExecTerminalMech(GameLink::sSharedMMapBuffer_R1* p_procbuf)
{
	proc_mech(p_procbuf, p_procbuf->payload);
}

void GameLink::ExecTerminal(GameLink::sSharedMMapBuffer_R1* p_inbuf,
	GameLink::sSharedMMapBuffer_R1* p_outbuf,
	GameLink::sSharedMMapBuffer_R1* p_procbuf)
{
	// Nothing from the host, or host hasn't acknowledged our last message.
	if (p_inbuf->payload == 0) {
		return;
	}
	if (p_outbuf->payload > 0) {
		return;
	}

	// Store output pointer
	g_p_outbuf = p_outbuf;

	// Process mode select ...
	if (p_inbuf->data[0] == ':')
	{
		// Acknowledge now, to avoid loops.
		UINT16 payload = p_inbuf->payload;
		p_inbuf->payload = 0;

		// Copy out.
		memcpy(p_procbuf->data, p_inbuf->data, payload);
		p_procbuf->payload = payload;
	}
}