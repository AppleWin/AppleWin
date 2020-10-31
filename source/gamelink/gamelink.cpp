
// Game Link

#include <Windows.h>
#include "Common.h"
#include "StdAfx.h"
#include "Log.h"
#include "gamelink.h"

//==============================================================================
//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------
bool g_bEnableGamelink;	// RIK Global to remember the state of GameLink
bool g_paused;			// RIK Not used now

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


static bool g_trackonly_mode;

static Bit32u g_membase_size;

static GameLink::sSharedMemoryMap_R4* g_p_shared_memory;

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

	g_p_shared_memory->program_hash[ 0 ] = 0;
	g_p_shared_memory->program_hash[ 1 ] = 0;
	g_p_shared_memory->program_hash[ 2 ] = 0;
	g_p_shared_memory->program_hash[ 3 ] = 0;

	// reset input
	g_p_shared_memory->input.mouse_dx = 0;
	g_p_shared_memory->input.mouse_dy = 0;

	g_p_shared_memory->input.mouse_btn = 0;
	for ( int i = 0; i < 8; ++i ) {
		g_p_shared_memory->input.keyb_state[ i ] = 0;
	}

	// reset peek interface
	g_p_shared_memory->peek.addr_count = 0;
	memset( g_p_shared_memory->peek.addr, 0, GameLink::sSharedMMapPeek_R2::PEEK_LIMIT * sizeof( Bit32u ) );
	memset( g_p_shared_memory->peek.data, 0, GameLink::sSharedMMapPeek_R2::PEEK_LIMIT * sizeof( Bit8u ) );

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
	g_trackonly_mode = trackonly_mode;

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
Bit8u* GameLink::AllocRAM( const Bit32u size )
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

	Bit8u* membase = ((Bit8u*)g_p_shared_memory) + MEMORY_MAP_CORE_SIZE;

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
// GameLink::In
//------------------------------------------------------------------------------
int GameLink::In( GameLink::sSharedMMapInput_R2* p_input,
				  GameLink::sSharedMMapAudio_R1* p_audio )
{
	int ready = 0;

	if ( g_p_shared_memory )
	{
		if ( g_trackonly_mode )
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

			// Volume sync, ignore if out of range.
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
void GameLink::Out( const Bit16u frame_width,
					const Bit16u frame_height,
					const double source_ratio,
					const bool want_mouse,
					const char* p_program,
					const Bit32u* p_program_hash,
					const Bit8u* p_frame,
					const Bit8u* p_sysmem )
{
	// Not initialised (or disabled) ?
	if ( g_p_shared_memory == NULL ) {
		return; // <=== EARLY OUT
	}

	// Create integer ratio
	Bit16u par_x, par_y;
	if ( source_ratio >= 1.0 )
	{
		par_x = 4096;
		par_y = static_cast< Bit16u >( source_ratio * 4096.0 );
	}
	else
	{
		par_x = static_cast< Bit16u >( 4096.0 / source_ratio );
		par_y = 4096;
	}

	// Build flags
	Bit8u flags;

	if ( g_trackonly_mode )
	{
		// Tracking Only - DOSBox handles video/input as usual.
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
	if ( g_paused )
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
//		printf( "GAMELINK: MUTEX lock ok\n" );

		{ // ========================

			// Set version
			g_p_shared_memory->version = PROTOCOL_VER;

			// Set program
			strncpy( g_p_shared_memory->program, p_program, 256 );
			for ( int i = 0; i < 4; ++i )
			{
				g_p_shared_memory->program_hash[ i ] = p_program_hash[ i ];
			}

			// Store flags
			g_p_shared_memory->flags = flags;

			if ( g_trackonly_mode == false )
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
				Bit32u payload;
				payload = frame_width * frame_height * 4;
				if ( frame_width <= sSharedMMapFrame_R1::MAX_WIDTH && frame_height <= sSharedMMapFrame_R1::MAX_HEIGHT )
				{
					memcpy( g_p_shared_memory->frame.buffer, p_frame, payload );
				}
			}

			// Peek
			for ( Bit32u pindex = 0;
					pindex < g_p_shared_memory->peek.addr_count &&
					pindex < sSharedMMapPeek_R2::PEEK_LIMIT;
				++pindex )
			{
				// read address
				Bit32u address;
				address = g_p_shared_memory->peek.addr[ pindex ];

				Bit8u data;
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

/*-
 *  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or
 *  code or tables extracted from it, as desired without restriction.
 */

static Bit32u crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

Bit32u GameLink::crc32(Bit32u crc, Bit8u* buf, Bit32u len)
{
	const Bit8u* p;

	p = buf;
	crc = crc ^ ~0U;

	while (len--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return crc ^ ~0U;
}

//==============================================================================


