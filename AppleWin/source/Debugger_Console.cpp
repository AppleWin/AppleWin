/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Debugger
 *
 * Author: Copyright (C) 2006, Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop

// Console ________________________________________________________________________________________

	// See ConsoleInputReset() for why the console input
	// is tied to the zero'th output of g_aConsoleDisplay
	// and not using a seperate var: g_aConsoleInput[ CONSOLE_WIDTH ];
	//
	//          :          g_aConsoleBuffer[4] |      ^ g_aConsoleDisplay[5]        :
	//          :          g_aConsoleBuffer[3] |      | g_aConsoleDisplay[4]  <-  g_nConsoleDisplayTotal
	// g_nConsoleBuffer -> g_aConsoleBuffer[2] |      | g_aConsoleDisplay[3]        :
	//          :          g_aConsoleBuffer[1] v      | g_aConsoleDisplay[2]        :
	//          .          g_aConsoleBuffer[0] -----> | g_aConsoleDisplay[1]        .
	//                                                | 
	// g_aBufferedInput[0] -----> ConsoleInput ---->  | g_aConsoleDisplay[0]   
	// g_aBufferedInput[1] ^
	// g_aBufferedInput[2] |
	// g_aBufferedInput[3] |
	
	// Buffer
		bool  g_bConsoleBufferPaused = false; // buffered output is waiting for user to continue
		int   g_nConsoleBuffer = 0;
		TCHAR g_aConsoleBuffer[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ]; // TODO: stl::vector< line_t >

	// Display
		TCHAR g_aConsolePrompt[] = TEXT(">!"); // input, assembler // NUM_PROMPTS
		TCHAR g_sConsolePrompt[] = TEXT(">"); // No, NOT Integer Basic!  The nostalgic '*' "Monitor" doesn't look as good, IMHO. :-(
		bool  g_bConsoleFullWidth = false;

		int   g_iConsoleDisplayStart  = 0; // to allow scrolling
		int   g_nConsoleDisplayTotal  = 0; // number of lines added to console
		int   g_nConsoleDisplayHeight = 0;
		int   g_nConsoleDisplayWidth  = 0;
		TCHAR g_aConsoleDisplay[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ];

	// Input History
		int   g_nHistoryLinesStart = 0;
		int   g_nHistoryLinesTotal = 0; // number of commands entered
		TCHAR g_aHistoryLines[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ] = {TEXT("")};

	// Input Line
		// Raw input Line (has prompt)
		TCHAR * const g_aConsoleInput = g_aConsoleDisplay[0];

		// Cooked input line (no prompt)
		int           g_nConsoleInputChars  = 0;
		TCHAR *       g_pConsoleInput       = 0; // points to past prompt
		const TCHAR * g_pConsoleFirstArg    = 0; // points to first arg
		bool          g_bConsoleInputQuoted = false; // Allows lower-case to be entered
		bool          g_bConsoleInputSkip   = false;

// Prototypes _______________________________________________________________

// Console ________________________________________________________________________________________

//===========================================================================
LPCSTR ConsoleBufferPeek()
{
	return g_aConsoleBuffer[ 0 ];
}


// Add string to buffered output
// Shifts the buffered console output lines "Up"
//===========================================================================
bool ConsoleBufferPush( const TCHAR * pString ) // LPCSTR
{
	if (g_nConsoleBuffer < CONSOLE_HEIGHT)
	{
		int nLen = _tcslen( pString );
		if (nLen < g_nConsoleDisplayWidth)
		{
			_tcscpy( g_aConsoleBuffer[ g_nConsoleBuffer ], pString );
			g_nConsoleBuffer++;
			return true;
		}
		else
		{
#if _DEBUG
//			TCHAR sText[ CONSOLE_WIDTH * 2 ];
//			sprintf( sText, "ConsoleBufferPush(pString) > g_nConsoleDisplayWidth: %d", g_nConsoleDisplayWidth );
//			MessageBox( framewindow, sText, "Warning", MB_OK );
#endif
			// push multiple lines
			while ((nLen >= g_nConsoleDisplayWidth) && (g_nConsoleBuffer < CONSOLE_HEIGHT))
			{
//				_tcsncpy( g_aConsoleBuffer[ g_nConsoleBuffer ], pString, (g_nConsoleDisplayWidth-1) );
//				pString += g_nConsoleDisplayWidth;
				_tcsncpy( g_aConsoleBuffer[ g_nConsoleBuffer ], pString, (CONSOLE_WIDTH-1) );
				pString += (CONSOLE_WIDTH-1);
				g_nConsoleBuffer++;
				nLen = _tcslen( pString );
			}
			return true;
		}
	}

	// TODO: Warning: Too much output.
	return false;
}

// Shifts the buffered console output "down"
//===========================================================================
void ConsoleBufferPop()
{
	int y = 0;
	while (y < g_nConsoleBuffer)
	{
		_tcscpy( g_aConsoleBuffer[ y ], g_aConsoleBuffer[ y+1 ] );
		y++;
	}

	g_nConsoleBuffer--;
	if (g_nConsoleBuffer < 0)
		g_nConsoleBuffer = 0;
}

// Remove string from buffered output
//===========================================================================
void ConsoleBufferToDisplay()
{
	ConsoleDisplayPush( ConsoleBufferPeek() );
	ConsoleBufferPop();
}

//===========================================================================
Update_t ConsoleDisplayError (LPCTSTR pText)
{
	ConsoleBufferPush( pText );
	return ConsoleUpdate();
}

// ConsoleDisplayPush()
// Shifts the console display lines "up"
//===========================================================================
void ConsoleDisplayPush( LPCSTR pText )
{
	int nLen = MIN( g_nConsoleDisplayTotal, CONSOLE_HEIGHT - 1 - CONSOLE_FIRST_LINE);
	while (nLen--)
	{
		_tcsncpy(
			g_aConsoleDisplay[(nLen + 1 + CONSOLE_FIRST_LINE )],
			g_aConsoleDisplay[nLen + CONSOLE_FIRST_LINE],
			CONSOLE_WIDTH );
	}

	if (pText)
		_tcsncpy( g_aConsoleDisplay[ CONSOLE_FIRST_LINE ], pText, CONSOLE_WIDTH );

	g_nConsoleDisplayTotal++;
	if (g_nConsoleDisplayTotal > (CONSOLE_HEIGHT - CONSOLE_FIRST_LINE))
		g_nConsoleDisplayTotal = (CONSOLE_HEIGHT - CONSOLE_FIRST_LINE);

}


//===========================================================================
void ConsoleDisplayPause()
{
	if (g_nConsoleBuffer)
	{
		_tcscpy( g_pConsoleInput, TEXT("...press SPACE continue, ESC skip..." ) );
		g_bConsoleBufferPaused = true;
	}
	else
	{
		ConsoleInputReset();
	}
}

//===========================================================================
bool ConsoleInputBackSpace()
{
	if (g_nConsoleInputChars)
	{
		g_nConsoleInputChars--;

		if (g_pConsoleInput[ g_nConsoleInputChars ] == TEXT('"'))
			g_bConsoleInputQuoted = ! g_bConsoleInputQuoted;

		g_pConsoleInput[ g_nConsoleInputChars ] = 0;
		return true;
	}
	return false;
}

//===========================================================================
bool ConsoleInputClear()
{
	if (g_nConsoleInputChars)
	{
		ZeroMemory( g_pConsoleInput, g_nConsoleDisplayWidth );
		g_nConsoleInputChars = 0;
		return true;
	}
	return false;
}

//===========================================================================
bool ConsoleInputChar( TCHAR ch )
{
	if (g_nConsoleInputChars < g_nConsoleDisplayWidth) // bug? include prompt?
	{
		g_pConsoleInput[ g_nConsoleInputChars ] = ch;
		g_nConsoleInputChars++;
		return true;
	}
	return false;
}

//===========================================================================
LPCSTR ConsoleInputPeek()
{
	return g_aConsoleDisplay[0];
}

//===========================================================================
void ConsoleInputReset ()
{
	// Not using g_aConsoleInput since we get drawing of the input Line for "Free"
	// Even if we add console scrolling, we don't need any special logic to draw the input line.
	g_bConsoleInputQuoted = false;

	ZeroMemory( g_aConsoleInput, CONSOLE_WIDTH );
	_tcscpy( g_aConsoleInput, g_sConsolePrompt ); // Assembler can change prompt
	_tcscat( g_aConsoleInput, TEXT(" " ) );

	int nLen = _tcslen( g_aConsoleInput );
	g_pConsoleInput = &g_aConsoleInput[nLen];
	g_nConsoleInputChars = 0;
}

//===========================================================================
int ConsoleInputTabCompletion ()
{
	return UPDATE_CONSOLE_INPUT;
}

//===========================================================================
Update_t ConsoleScrollHome ()
{
	g_iConsoleDisplayStart = g_nConsoleDisplayTotal - CONSOLE_FIRST_LINE;
	if (g_iConsoleDisplayStart < 0)
		g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollEnd ()
{
	g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollUp ( int nLines )
{
	g_iConsoleDisplayStart += nLines;

	if (g_iConsoleDisplayStart > (g_nConsoleDisplayTotal - CONSOLE_FIRST_LINE))
		g_iConsoleDisplayStart = (g_nConsoleDisplayTotal - CONSOLE_FIRST_LINE);

	if (g_iConsoleDisplayStart < 0)
		g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollDn ( int nLines )
{
	g_iConsoleDisplayStart -= nLines;
	if (g_iConsoleDisplayStart < 0)
		g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollPageUp ()
{
	ConsoleScrollUp( g_nConsoleDisplayHeight - CONSOLE_FIRST_LINE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollPageDn()
{
	ConsoleScrollDn( g_nConsoleDisplayHeight - CONSOLE_FIRST_LINE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
void ConsoleBufferTryUnpause (int nLines)
{
	for( int y = 0; y < nLines; y++ )
	{
		ConsoleBufferToDisplay();
	}

	g_bConsoleBufferPaused = false;
	if (g_nConsoleBuffer)
	{
		g_bConsoleBufferPaused = true;
		ConsoleDisplayPause();
	}
}

//===========================================================================
Update_t ConsoleUpdate()
{
	if (! g_bConsoleBufferPaused)
	{
		int nLines = MIN( g_nConsoleBuffer, g_nConsoleDisplayHeight - 1);
		ConsoleBufferTryUnpause( nLines );
	}

	return UPDATE_CONSOLE_DISPLAY;
}
