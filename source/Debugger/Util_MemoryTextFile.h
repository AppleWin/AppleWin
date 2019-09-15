#pragma once

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
			m_vBuffer.erase( m_vBuffer.begin(), m_vBuffer.end() );
			m_vLines.erase( m_vLines.begin(), m_vLines.end() );
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

		void PushLine( char *pLine );
	};
	
