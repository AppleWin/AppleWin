/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

#include "StdAfx.h"

#include "Util_Text.h"
#include "Util_MemoryTextFile.h"

// MemoryTextFile _________________________________________________________________________________

const int EOL_NULL = 0;

//===========================================================================
bool MemoryTextFile_t::Read( const std::string & pFileName )
{
	bool bStatus = false;
	FILE *hFile = fopen( pFileName.c_str(), "rb" );

	if (hFile)
	{
		fseek( hFile, 0, SEEK_END );
		long nSize = ftell( hFile );
		fseek( hFile, 0, SEEK_SET );

		m_vBuffer.reserve( nSize + 1 );
		m_vBuffer.insert( m_vBuffer.begin(), nSize+1, 0 ); // NOTE: Can NOT m_vBuffer.clear(); MUST insert() _before_ using at()

		char *pBuffer = & m_vBuffer.at(0);
		fread( (void*)pBuffer, nSize, 1, hFile );
		fclose(hFile);

		m_bDirty = true;
		GetLinePointers();

		bStatus = true;
	}

	return bStatus;
}


//===========================================================================
void MemoryTextFile_t::GetLine( const int iLine, char *pLine, const int nMaxLineChars )
{
	if (m_bDirty)
	{
		GetLinePointers();
	}		

	memset( pLine, 0, nMaxLineChars );
	strncpy( pLine, m_vLines[ iLine ], nMaxLineChars-1 );
}


// cr/new lines are converted into null, string terminators
//===========================================================================
void MemoryTextFile_t::GetLinePointers()
{
	if (! m_bDirty)
		return;

	m_vLines.erase( m_vLines.begin(), m_vLines.end() );
	char *pBegin = & m_vBuffer[ 0 ];
	char *pLast  = & m_vBuffer[ m_vBuffer.size()-1 ];

	char *pEnd = NULL;
	char *pStartNextLine;

	while (pBegin <= pLast)
	{
		if ( *pBegin )	// Only keep non-empty lines
			m_vLines.push_back( pBegin );

		pEnd = const_cast<char*>( SkipUntilEOL( pBegin ));

		if (*pEnd == EOL_NULL)
		{
			// Found EOL via null
			pStartNextLine = pEnd + 1;
		}
		else
		{
			pStartNextLine = const_cast<char*>( EatEOL( pEnd ));

			// DOS/Win "Text" mode converts LF CR (0D 0A) to CR (0D)
			// but just in case, the file is read in binary.
			int nEOL = pStartNextLine - pEnd;
			while (nEOL-- > 1)
			{
				*pEnd++ = ' ';
			}

			// assert( pEnd != NULL	);
			*pEnd = EOL_NULL;
		}
		pBegin = pStartNextLine;
	}			

	m_bDirty = false;
}


//===========================================================================
void MemoryTextFile_t::PushLine( char *pLine )
{
	char *pSrc = pLine;
	while (pSrc && *pSrc)
	{
		if (*pSrc == CHAR_CR)
			m_vBuffer.push_back( EOL_NULL );
		else
		if (*pSrc == CHAR_LF)			
			m_vBuffer.push_back( EOL_NULL );
		else
			m_vBuffer.push_back( *pSrc );

		pSrc++;		
	}
	m_vBuffer.push_back( EOL_NULL );

	m_bDirty = true;
}


