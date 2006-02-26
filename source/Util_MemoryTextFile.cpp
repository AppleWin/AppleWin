#include "StdAfx.h"
#pragma  hdrstop

// MemoryTextFile _________________________________________________________________________________

const int EOL_TERM = 0;

//===========================================================================
bool MemoryTextFile_t::Read( char *pFileName )
{
	bool bStatus = false;
	FILE *hFile = fopen( pFileName, "rt" );

	if (hFile)
	{
		fseek( hFile, 0, SEEK_END );
		long nSize = ftell( hFile );
		fseek( hFile, 0, SEEK_SET );

		m_vBuffer.reserve( nSize + 1 );
		m_vBuffer.insert( m_vBuffer.begin(), nSize+1, 0 );

		char *pBuffer = & m_vBuffer.at(0);
		fread( (void*)pBuffer, nSize, 1, hFile );

		m_vBuffer.push_back( EOL_TERM );

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

	ZeroMemory( pLine, nMaxLineChars );
	strncpy( pLine, m_vLines[ iLine ], nMaxLineChars-1 );
}


// cr/new lines are converted into null, string terminators
//===========================================================================
void MemoryTextFile_t::GetLinePointers()
{
	if (! m_bDirty)
		return;

	m_vLines.erase( m_vLines.begin(), m_vLines.end() );
	char *pBegin = & m_vBuffer.at( 0 );
	char *pLast  = & m_vBuffer[ m_vBuffer.size() ];

	char *pEnd = NULL;
	char *pStartNextLine;

	while (pBegin < pLast)
	{
		m_vLines.push_back( pBegin );

		pEnd = const_cast<char*>( SkipUntilEOL( pBegin ));

		if (*pEnd != EOL_TERM)
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
			*pEnd = EOL_TERM;
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
			m_vBuffer.push_back( EOL_TERM );
		else
		if (*pSrc == CHAR_LF)			
			m_vBuffer.push_back( EOL_TERM );
		else
			m_vBuffer.push_back( *pSrc );

		pSrc++;		
	}
	m_vBuffer.push_back( EOL_TERM );

	m_bDirty = true;
}


