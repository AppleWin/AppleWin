#pragma once

#include "StrFormat.h"

// Memory Text File _________________________________________________________

	class MemoryTextFile_t
	{
		std::vector<char  > m_vBuffer;
		std::vector<char *> m_vLines ; // array of pointers to start of lines
		bool           m_bDirty ; // line pointers not up-to-date

		void GetLinePointers();

	public:
		MemoryTextFile_t()
//		: m_nSize( 0 )
//		, m_pBuffer( 0 )
//		, m_nLines( 0 )
		: m_bDirty( false )
		{
			m_vBuffer.reserve( 2048 );
			m_vLines.reserve( 128 );				
		}

		bool Read( const std::string & pFileName );
		void Reset()
		{
			m_vBuffer.clear();
			m_vLines.clear();
		}

inline	int  GetNumLines()
		{
			if (m_bDirty)
				GetLinePointers();

			return m_vLines.size();
		}

inline	char *GetLine( const int iLine ) const
		{
			return m_vLines.at( iLine );
		}

		void  GetLine( const int iLine, char *pLine, const int n );

		void PushLine( const char *pLine );
		void PushLineFormat( const char *pFormat, ... ) ATTRIBUTE_FORMAT_PRINTF(2, 3); // 1 is "this"
	};
	
