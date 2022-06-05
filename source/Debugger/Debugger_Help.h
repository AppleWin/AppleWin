#ifndef DEBUGGER_HELP_H
#define DEBUGGER_HELP_H

	enum Match_e
	{
		MATCH_EXACT,
		MATCH_FUZZY
	};


// Prototypes _______________________________________________________________

	Update_t HelpLastCommand();

	void DisplayAmbigiousCommands ( int nFound );

	int FindParam( LPCTSTR pLookupName, Match_e eMatch, int & iParam_, const int iParamBegin = 0, const int iParamEnd = NUM_PARAMS - 1 );
	int FindCommand( LPCTSTR pName, CmdFuncPtr_t & pFunction_, int * iCommand_ = NULL );

inline void  UnpackVersion( const unsigned int nVersion,
		int & nMajor_, int & nMinor_, int & nFixMajor_ , int & nFixMinor_ )
	{
		nMajor_    = (nVersion >> 24) & 0xFF;
		nMinor_    = (nVersion >> 16) & 0xFF;
		nFixMajor_ = (nVersion >>  8) & 0xFF;
		nFixMinor_ = (nVersion >>  0) & 0xFF;
	}

#endif
