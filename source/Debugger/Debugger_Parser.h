#ifndef DEBUGGER_PARSER_H
#define DEBUGGER_PARSER_H

#include "Util_Text.h"

const char * ParserFindToken( const char *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e * pToken_ );
const TCHAR * FindTokenOrAlphaNumeric ( const TCHAR *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e * pToken_ );
int RemoveWhiteSpaceReverse( char *pSrc );
void TextConvertTabsToSpaces( TCHAR *pDeTabified_, LPCTSTR pText, const int nDstSize, int nTabStop = 0 );

inline const char* SkipUntilToken( const char *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e *pToken_ )
	{
		if ( pToken_)
			*pToken_ = NO_TOKEN;

		while (pSrc && (*pSrc))
		{
			if (ParserFindToken( pSrc, aTokens, nTokens, pToken_ ))
				return pSrc;

			pSrc++;
		}
		return pSrc;
	}

// Globals __________________________________________________________________

	extern	int   g_nArgRaw;
	extern	Arg_t g_aArgRaw[ MAX_ARGS ]; // pre-processing
	extern	Arg_t g_aArgs  [ MAX_ARGS ]; // post-processing

	extern	const TCHAR * g_pConsoleFirstArg; //    = 0; // points to first arg

	extern	const TokenTable_t g_aTokens[ NUM_TOKENS ];

	extern	const TCHAR TCHAR_LF    ;//= 0x0D;
	extern	const TCHAR TCHAR_CR    ;//= 0x0A;
	extern	const TCHAR TCHAR_SPACE ;//= TEXT(' ');
	extern	const TCHAR TCHAR_TAB   ;//= TEXT('\t');
	extern	const TCHAR TCHAR_QUOTE_DOUBLE;
	extern	const TCHAR TCHAR_QUOTE_SINGLE;

// Prototypes _______________________________________________________________

// Arg - Command Processing
	Update_t Help_Arg_1( int iCommandHelp );
	int _Arg_1     ( int nValue );
	int _Arg_1     ( LPTSTR pName );
	int _Arg_Shift ( int iSrc, int iEnd, int iDst = 0 );
	int _Args_Insert( int iSrc, int iEnd, int nLen );
	void ArgsClear ();

	bool ArgsGetValue ( Arg_t *pArg, WORD * pAddressValue_, const int nBase = 16 );
	bool ArgsGetImmediateValue ( Arg_t *pArg, WORD * pAddressValue_ );
	int	 ArgsGet ( TCHAR * pInput );
	bool ArgsGetRegisterValue ( Arg_t *pArg, WORD * pAddressValue_ );
	void ArgsRawParse ( void );
	int ArgsCook ( const int nArgs ); // const int bProcessMask );

#endif
