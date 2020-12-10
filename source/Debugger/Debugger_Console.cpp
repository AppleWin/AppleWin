/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski

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

/* Description: Debugger Console support
 *
 * Author: Copyright (C) 2006 - 2010 Michael Pohoreski
 */

#include "StdAfx.h"

#include "Debug.h"

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
		bool      g_bConsoleBufferPaused = false; // buffered output is waiting for user to continue
		int       g_nConsoleBuffer = 0;
		conchar_t g_aConsoleBuffer[ CONSOLE_BUFFER_HEIGHT ][ CONSOLE_WIDTH ]; // TODO: std::vector< line_t >

	// Cursor
		char      g_sConsoleCursor[] = "_";
	
	// Display
		char      g_aConsolePrompt[] = ">!"; // input, assembler // NUM_PROMPTS
		char      g_sConsolePrompt[] = ">"; // No, NOT Integer Basic!  The nostalgic '*' "Monitor" doesn't look as good, IMHO. :-(
		int       g_nConsolePromptLen = 1;

		bool      g_bConsoleFullWidth = true; // false

		int       g_iConsoleDisplayStart  = 0; // to allow scrolling
		int       g_nConsoleDisplayTotal  = 0; // number of lines added to console
		int       g_nConsoleDisplayLines  = 0;
		int       g_nConsoleDisplayWidth  = 0;
		conchar_t g_aConsoleDisplay[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ];

	// Input History
		int   g_nHistoryLinesStart = 0;
		int   g_nHistoryLinesTotal = 0; // number of commands entered
		char  g_aHistoryLines[ HISTORY_HEIGHT ][ HISTORY_WIDTH ] = {""};

	// Input Line

		// Raw input Line (has prompt)
		char g_aConsoleInput[ CONSOLE_WIDTH ]; // = g_aConsoleDisplay[0];

		// Cooked input line (no prompt)
		int          g_nConsoleInputChars  = 0;
		      char * g_pConsoleInput       = 0; // points to past prompt
		const char * g_pConsoleFirstArg    = 0; // points to first arg
		bool         g_bConsoleInputQuoted = false; // Allows lower-case to be entered
		char         g_nConsoleInputSkip   = '~';

// Prototypes _______________________________________________________________

// Console ________________________________________________________________________________________

int ConsoleLineLength( const conchar_t * pText )
{
	int nLen = 0;
	const conchar_t *pSrc = pText;

	if (pText )
	{
		while (*pSrc)
		{
			pSrc++;
		}
		nLen = pSrc - pText;
	}
	return nLen;
}


//===========================================================================
const conchar_t* ConsoleBufferPeek ()
{
	return g_aConsoleBuffer[ 0 ];
}


//===========================================================================
bool ConsolePrint ( const char * pText )
{
	while (g_nConsoleBuffer >= CONSOLE_BUFFER_HEIGHT)
	{
		ConsoleBufferToDisplay();	
	}

	// Convert color string to native console color text
	// Ignores g_nConsoleDisplayWidth
	char c;

	int x = 0;
	const char *pSrc = pText;
	conchar_t  *pDst = & g_aConsoleBuffer[ g_nConsoleBuffer ][ 0 ];

	conchar_t g = 0;
	bool bHaveColor = false;
	char cColor = 0;

	while ((x < CONSOLE_WIDTH) && (c = *pSrc))
	{
		if ((c == '\n') || (x >= (CONSOLE_WIDTH - 1)))
		{
			*pDst = 0;
			x = 0;
			if (g_nConsoleBuffer >= CONSOLE_BUFFER_HEIGHT)
			{
				ConsoleBufferToDisplay();	
			}
			else
			{
				g_nConsoleBuffer++;
			}							
			pDst = & g_aConsoleBuffer[ g_nConsoleBuffer ][ 0 ];
		}
		else
		{
			g = (c & _CONSOLE_COLOR_MASK);

			// `# `A  color encode mouse text
			if (ConsoleColor_IsCharMeta( c ))
			{
				if (! pSrc[1])
					break;

				if (ConsoleColor_IsCharMeta( pSrc[1] )) // ` `
				{
					bHaveColor = false;
					cColor = 0;
					g = ConsoleColor_MakeColor( cColor, c );
					*pDst = g;
					x++;
					pDst++;
				}
				else
				if (ConsoleColor_IsCharColor( pSrc[1] )) // ` #
				{
					cColor = pSrc[1];
					bHaveColor = true;
				}
				else // ` @
				{
					c = ConsoleColor_MakeMouse( pSrc[1] );
					g = ConsoleColor_MakeColor( cColor, c );
					*pDst = g;
					x++;
					pDst++;
				}
				pSrc++;
				pSrc++;
			}
			else
			{
				if (bHaveColor)
				{
					g = ConsoleColor_MakeColor( cColor, c );	
					bHaveColor = false;
				}
				*pDst = g;
				x++;
				pDst++;
				pSrc++;
			}
		}
/*
		if (ConsoleColor_IsCharMeta( c ))
		{
			// Convert mult-byte to packed char
			//    0 1 2 Offset
			//    =====
			// 1  ~ -   null
			// 2  ~ 0 - null - exit
			// 3  ~ 0 x color (3 bytes packed into char16
			// 4  ~ @ - mouse text
			// 5  ~ @ x mouse Text
			// 6  ~ ~   ~
			// Legend:
			//	~  Color escape
			//  x  Any char
			//  -  Null
			if (pSrc[1])
			{
				if (ConsoleColor_IsCharMeta( pSrc[1] )) // 6
				{
					*pDst = c;
					x++;
					pSrc += 2;
					pDst++;
				}
				else
				if (ConsoleColor_IsCharColor( pSrc[1] ))
				{
					if (pSrc[2]) // 3
					{
						x++;
						*pDst = ConsoleColor_MakeColor( pSrc[1], pSrc[2] );
						pSrc += 3;
						pDst++;
					}
					else
						break; // 2
				}
				else // 4 or 5
				{
					*pDst = ConsoleColor_MakeMeta( pSrc[1] );
					x++;
					pSrc += 2;
					pDst++;
				}
			}
			else
				break; // 1
		}
		else
		{
			*pDst = (c & _CONSOLE_COLOR_MASK);
			x++;
			pSrc++;
			pDst++;
		}
*/
	}
	*pDst = 0;
	g_nConsoleBuffer++;

	return true;
}

bool ConsolePrintVa ( char* buf, size_t bufsz, const char * pFormat, va_list va )
{
	vsnprintf_s(buf, bufsz, _TRUNCATE, pFormat, va);
	return ConsolePrint(buf);
}

bool ConsoleBufferPushVa ( char* buf, size_t bufsz, const char * pFormat, va_list va )
{
	vsnprintf_s(buf, bufsz, _TRUNCATE, pFormat, va);
	return ConsoleBufferPush(buf);
}

// Add string to buffered output
// Shifts the buffered console output lines "Up"
//===========================================================================
bool ConsoleBufferPush ( const char * pText )
{
	while (g_nConsoleBuffer >= CONSOLE_BUFFER_HEIGHT)
	{
		ConsoleBufferToDisplay();
	}

	conchar_t c;

	int x = 0;
	const char *pSrc = pText;
	conchar_t  *pDst = & g_aConsoleBuffer[ g_nConsoleBuffer ][ 0 ];

	while ((x < CONSOLE_WIDTH) && *pSrc)
	{
		c = *pSrc;
		if ((c == '\n') || (x == (CONSOLE_WIDTH - 1)))
		{
			*pDst = 0;
			x = 0;
			if (g_nConsoleBuffer >= CONSOLE_BUFFER_HEIGHT)
			{
				ConsoleBufferToDisplay();	
			}
			else
			{
				g_nConsoleBuffer++;
			}
			pSrc++;
			pDst = & g_aConsoleBuffer[ g_nConsoleBuffer ][ 0 ];
		}
		else
		{
			*pDst = (c & _CONSOLE_COLOR_MASK);
			x++;
			pSrc++;
			pDst++;
		}
	}
	*pDst = 0;
	g_nConsoleBuffer++;

	return true;
}

// Shifts the buffered console output "down"
//===========================================================================
void ConsoleBufferPop ()
{
	int y = 0;
	while (y < g_nConsoleBuffer)
	{
		memcpy(
			g_aConsoleBuffer[ y ],
			g_aConsoleBuffer[ y+1 ],
			sizeof( conchar_t ) * CONSOLE_WIDTH
		);
		y++;
	}

	g_nConsoleBuffer--;
	if (g_nConsoleBuffer < 0)
		g_nConsoleBuffer = 0;
}

// Remove string from buffered output
//===========================================================================
void ConsoleBufferToDisplay ()
{
	ConsoleDisplayPush( ConsoleBufferPeek() );
	ConsoleBufferPop();
}

// No mark-up. Straight ASCII conversion
//===========================================================================
void ConsoleConvertFromText ( conchar_t * sText, const char * pText )
{
	const char *pSrc = pText;
	conchar_t  *pDst = sText;
	while (pSrc && *pSrc)
	{
		*pDst = (conchar_t) (*pSrc & _CONSOLE_COLOR_MASK);
		pSrc++;
		pDst++;
	}
	*pDst = 0;
}

//===========================================================================
Update_t ConsoleDisplayError ( const char * pText)
{
	ConsoleBufferPush( pText );
	return ConsoleUpdate();
}


//===========================================================================
void ConsoleDisplayPush ( const char * pText )
{
	conchar_t sText[ CONSOLE_WIDTH * 2 ];
	ConsoleConvertFromText( sText, pText );
	ConsoleDisplayPush( sText );
}


// Shifts the console display lines "up"
//===========================================================================
void ConsoleDisplayPush ( const conchar_t * pText )
{
	int nLen = MIN( g_nConsoleDisplayTotal, CONSOLE_HEIGHT - 1 - CONSOLE_FIRST_LINE);
	while (nLen--)
	{
		memcpy(
			  (char*) g_aConsoleDisplay[(nLen + 1 + CONSOLE_FIRST_LINE )]
			, (char*) g_aConsoleDisplay[nLen + CONSOLE_FIRST_LINE]
			, sizeof(conchar_t) * CONSOLE_WIDTH
		);
	}

	if (pText)
	{
		memcpy(
			  (char*) g_aConsoleDisplay[ CONSOLE_FIRST_LINE ]
			, pText
			, sizeof(conchar_t) * CONSOLE_WIDTH
		);
	}
	
	g_nConsoleDisplayTotal++;
	if (g_nConsoleDisplayTotal > (CONSOLE_HEIGHT - CONSOLE_FIRST_LINE))
		g_nConsoleDisplayTotal = (CONSOLE_HEIGHT - CONSOLE_FIRST_LINE);

}


//===========================================================================
void ConsoleDisplayPause ()
{
	if (g_nConsoleBuffer)
	{
#if CONSOLE_INPUT_CHAR16
		ConsoleConvertFromText(
			g_aConsoleInput,
			"...press SPACE continue, ESC skip..."
		);
		g_nConsolePromptLen = ConsoleLineLength( g_pConsoleInput ) + 1;
#else
		strcpy(
			g_aConsoleInput,
			"...press SPACE continue, ESC skip..."
		);
		g_nConsolePromptLen = strlen( g_pConsoleInput ) + 1;
#endif
		g_nConsoleInputChars = 0;
		g_bConsoleBufferPaused = true;
	}
	else
	{
		ConsoleInputReset();
	}
}

//===========================================================================
bool ConsoleInputBackSpace ()
{
	if (g_nConsoleInputChars)
	{
		g_pConsoleInput[ g_nConsoleInputChars ] = CHAR_SPACE;

		g_nConsoleInputChars--;

		if ((g_pConsoleInput[ g_nConsoleInputChars ] == CHAR_QUOTE_DOUBLE) ||
			(g_pConsoleInput[ g_nConsoleInputChars ] == CHAR_QUOTE_SINGLE))
			g_bConsoleInputQuoted = ! g_bConsoleInputQuoted;

		g_pConsoleInput[ g_nConsoleInputChars ] = CHAR_SPACE;
		return true;
	}
	return false;
}

// Clears prompt too
//===========================================================================
bool ConsoleInputClear ()
{
	memset( g_aConsoleInput, 0, CONSOLE_WIDTH );

	if (g_nConsoleInputChars)
	{
		g_nConsoleInputChars = 0;
		return true;
	}
	return false;
}

//===========================================================================
bool ConsoleInputChar ( const char ch )
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
void ConsoleUpdateCursor ( char ch )
{
	if (ch)
		g_sConsoleCursor[0] = ch;
	else
	{
		ch = (char) g_aConsoleInput[ g_nConsoleInputChars + g_nConsolePromptLen ];
		if (! ch)
		{
			ch = CHAR_SPACE;
		}
		g_sConsoleCursor[0] = ch;
	}
}


//===========================================================================
const char * ConsoleInputPeek ()
{
//	return g_aConsoleDisplay[0];
//	return g_pConsoleInput;
	return g_aConsoleInput;
}

//===========================================================================
void ConsoleInputReset ()
{
	// Not using g_aConsoleInput since we get drawing of the input Line for "Free"
	// Even if we add console scrolling, we don't need any special logic to draw the input line.
	g_bConsoleInputQuoted = false;

	ConsoleInputClear();

//	_tcscpy( g_aConsoleInput, g_sConsolePrompt ); // Assembler can change prompt
	g_aConsoleInput[0] = g_sConsolePrompt[0];
	g_nConsolePromptLen = 1;

	g_pConsoleInput = &g_aConsoleInput[ g_nConsolePromptLen ];
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
	ConsoleScrollUp( g_nConsoleDisplayLines - CONSOLE_FIRST_LINE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollPageDn ()
{
	ConsoleScrollDn( g_nConsoleDisplayLines - CONSOLE_FIRST_LINE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleBufferTryUnpause (int nLines)
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
		return UPDATE_CONSOLE_INPUT | UPDATE_CONSOLE_DISPLAY;
	}

	return UPDATE_CONSOLE_DISPLAY;
}

// Flush the console
//===========================================================================
Update_t ConsoleUpdate ()
{
	if (! g_bConsoleBufferPaused)
	{
		int nLines = MIN( g_nConsoleBuffer, g_nConsoleDisplayLines - 1);
		return ConsoleBufferTryUnpause( nLines );
	}

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
void ConsoleFlush ()
{
	int nLines = g_nConsoleBuffer;
	ConsoleBufferTryUnpause( nLines );
}
