
// Game Link - Console

#include "Common.h"
#include "StdAfx.h"
#include <stdio.h>
#include "gamelink.h"

using namespace GameLink;

typedef GameLink::sSharedMMapBuffer_R1	Buffer;

//==============================================================================

//------------------------------------------------------------------------------
// Local Data
//------------------------------------------------------------------------------

static Buffer* g_p_outbuf;


//------------------------------------------------------------------------------
// Local Functions
//------------------------------------------------------------------------------

//
// proc_mech
//
// Process a mechanical command - encoded form for computer-computer communication. Minimal feedback.
//
static void proc_mech( Buffer* cmd, Bit16u payload )
{
	// Ignore NULL commands.
	if ( payload <= 1 || payload > 128 )
		return;

	cmd->payload = 0;
	char* com = (char*)(cmd->data);
	com[ payload ] = 0;

//	printf( "command = %s; payload = %d\n", com, payload );

	//
	// Decode

	if ( strcmp( com, ":reset" ) == 0 )
	{
//		printf( "do reset\n" );

		// RIK disable reset
	}
	else if ( strcmp( com, ":pause" ) == 0 )
	{
//		printf( "do pause\n" );

		// RIK disable pause
	}
	else if ( strcmp( com, ":shutdown" ) == 0 )
	{
//		printf( "do shutdown\n" );

		// RIK disable shutdown
//		extern void Shutdown(void);
//		Shutdown();
	}
}

//------------------------------------------------------------------------------
// Global Functions
//------------------------------------------------------------------------------

void GameLink::InitTerminal()
{
	g_p_outbuf = 0;
}

void GameLink::ExecTerminalMech( Buffer* p_procbuf )
{
	proc_mech( p_procbuf, p_procbuf->payload );
}

void GameLink::ExecTerminal( Buffer* p_inbuf, 
							 Buffer* p_outbuf,
							 Buffer* p_procbuf )
{
	// Nothing from the host, or host hasn't acknowledged our last message.
	if ( p_inbuf->payload == 0 ) {
		return;
	}
	if ( p_outbuf->payload > 0 ) {
		return;
	}

	// Store output pointer
	g_p_outbuf = p_outbuf;

	// Process mode select ...
	if ( p_inbuf->data[ 0 ] == ':' )
	{
		// Acknowledge now, to avoid loops.
		Bit16u payload = p_inbuf->payload;
		p_inbuf->payload = 0;

		// Copy out.
		memcpy( p_procbuf->data, p_inbuf->data, payload );
		p_procbuf->payload = payload;
	}
	else
	{
		// Human command
		char buf[ Buffer::BUFFER_SIZE + 1 ], *b = buf;

		// Convert into printable ASCII
		for ( Bit32u i = 0; i < p_inbuf->payload; ++i )
		{
			Bit8u u8 = p_inbuf->data[ i ];
			if ( u8 < 32 || u8 > 127 ) {
				*b++ = '?';
			} else {
				*b++ = (char)u8;
			}
		}

		// ... terminate
		*b++ = 0;

		// Acknowledge
		p_inbuf->payload = 0;

		// proc_human( buf ); // <-- deprecated
	}
}

//==============================================================================


