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

/* Description: Debugger
 *
 * Author: Copyright (C) 2006, Michael Pohoreski
 */

#include "StdAfx.h"

#include "Debug.h"

#include "../CPU.h"
#include "../Memory.h"

// Args ___________________________________________________________________________________________

	int   g_nArgRaw;
	Arg_t g_aArgRaw[ MAX_ARGS ]; // pre-processing
	Arg_t g_aArgs  [ MAX_ARGS ]; // post-processing (cooked)

	const TCHAR TCHAR_LF     = TEXT('\x0D');
	const TCHAR TCHAR_CR     = TEXT('\x0A');
	const TCHAR TCHAR_SPACE  = TEXT(' ');
	const TCHAR TCHAR_TAB    = TEXT('\t');
//	const TCHAR TCHAR_QUOTED = TEXT('"');
	const TCHAR TCHAR_QUOTE_DOUBLE = TEXT('"');
	const TCHAR TCHAR_QUOTE_SINGLE = TEXT('\'');
	const TCHAR TCHAR_ESCAPE = TEXT('\x1B');


	// NOTE: ArgToken_e and g_aTokens must match!
	const TokenTable_t g_aTokens[ NUM_TOKENS ] =
	{ // Input
		{ TOKEN_ALPHANUMERIC, TYPE_STRING  , 0    }, // Default, if doen't match anything else
		{ TOKEN_AMPERSAND   , TYPE_OPERATOR, "&"  }, // bit-and
		{ TOKEN_AT          , TYPE_OPERATOR, "@"  }, // reference results 
		{ TOKEN_BRACE_L     , TYPE_STRING  , "{"  },
		{ TOKEN_BRACE_R     , TYPE_STRING  , "}"  },
		{ TOKEN_BRACKET_L   , TYPE_STRING  , "["  },
		{ TOKEN_BRACKET_R   , TYPE_STRING  , "]"  },
		{ TOKEN_BSLASH      , TYPE_OPERATOR, "\\" },
		{ TOKEN_CARET       , TYPE_OPERATOR, "^"  }, // bit-eor, C/C++: xor, Math: POWER
		{ TOKEN_COLON       , TYPE_OPERATOR, ":"  }, 
		{ TOKEN_COMMA       , TYPE_OPERATOR, ","  },
		{ TOKEN_DOLLAR      , TYPE_STRING  , "$"  },
		{ TOKEN_EQUAL       , TYPE_OPERATOR, "="  },
		{ TOKEN_EXCLAMATION , TYPE_OPERATOR, "!"  }, // NOT
		{ TOKEN_FSLASH      , TYPE_OPERATOR, "/"  }, // div
		{ TOKEN_GREATER_THAN, TYPE_OPERATOR, ">"  }, // TODO/FIXME: Parser will break up '>=' (needed for uber breakpoints)
		{ TOKEN_HASH        , TYPE_OPERATOR, "#"  },
		{ TOKEN_LESS_THAN   , TYPE_OPERATOR, "<"  },
		{ TOKEN_MINUS       , TYPE_OPERATOR, "-"  }, // sub
		{ TOKEN_PAREN_L     , TYPE_OPERATOR, "("  },
		{ TOKEN_PAREN_R     , TYPE_OPERATOR, ")"  },
		{ TOKEN_PERCENT     , TYPE_OPERATOR, "%"  }, // mod
		{ TOKEN_PIPE        , TYPE_OPERATOR, "|"  }, // bit-or
		{ TOKEN_PLUS        , TYPE_OPERATOR, "+"  }, // add
//		{ TOKEN_QUESTION    , TYPE_OPERATOR, TEXT('?')  }, // Not a token 1) wildcard needs to stay together with other chars
		{ TOKEN_QUOTE_SINGLE, TYPE_QUOTED_1, "\'" },
		{ TOKEN_QUOTE_DOUBLE, TYPE_QUOTED_2, "\"" }, // for strings
		{ TOKEN_SEMI        , TYPE_STRING  , ";"  },
		{ TOKEN_SPACE       , TYPE_STRING  , " "  }, // space is also a delimiter between tokens/args
		{ TOKEN_STAR        , TYPE_OPERATOR, "*"  }, // Not a token 1) wildcard needs to stay together with other chars
//		{ TOKEN_TAB         , TYPE_STRING  , TEXT('\t') }
		{ TOKEN_TILDE       , TYPE_OPERATOR, "~"  }, // C/C++: Not.  Used for console.

		{ TOKEN_COMMENT_EOL , TYPE_STRING  , "//" },
		{ TOKEN_GREATER_EQUAL,TYPE_OPERATOR, ">=" },
		{ TOKEN_LESS_EQUAL  , TYPE_OPERATOR, "<=" },
		{ TOKEN_NOT_EQUAL  , TYPE_OPERATOR , "!=" }
	};

// Arg ____________________________________________________________________________________________


//===========================================================================
int _Arg_1( int nValue )
{
	g_aArgs[1].nValue = nValue;
	return 1;
}
	
//===========================================================================
int _Arg_1( LPTSTR pName )
{
	int nLen = _tcslen( g_aArgs[1].sArg );
	if (nLen < MAX_ARG_LEN)
	{
		_tcscpy( g_aArgs[1].sArg, pName );
	}
	else
	{
		_tcsncpy( g_aArgs[1].sArg, pName, MAX_ARG_LEN );
	}
	return 1;
}

/**
	@description Copies Args[iSrc .. iEnd] to Args[0]
	@param iSrc First argument to copy
	@param iEnd Last argument to end
	@return nArgs Number of new args
	Usually called as: nArgs = _Arg_Shift( iArg, nArgs );
//=========================================================================== */
int _Arg_Shift( int iSrc, int iEnd, int iDst )
{
	if (iDst < 0)
		return ARG_SYNTAX_ERROR;
	if (iDst >= MAX_ARGS)
		return ARG_SYNTAX_ERROR;

	int nArgs = (iEnd - iSrc);
	int nLen = nArgs + 1;

	if ((iDst + nLen) > MAX_ARGS)
		return ARG_SYNTAX_ERROR;

	while (nLen--)
	{
		g_aArgs[iDst] = g_aArgs[iSrc];
		iSrc++;
		iDst++;
	}
	return nArgs;
}

//===========================================================================
int _Args_Insert( int iSrc, int iEnd, int nLen )
{
	iSrc += nLen;
	int iDst = iEnd + nLen;

	if (iDst >= MAX_ARGS)
		return ARG_SYNTAX_ERROR;

	if (iSrc >= MAX_ARGS)
		return ARG_SYNTAX_ERROR;
	
	while (nLen--)
	{
		g_aArgs[iDst] = g_aArgs[iSrc];
		iSrc--;
		iDst--;
	}

	return 0;
}


static void ClearArg( Arg_t *pArg )
{
	pArg->sArg[0] = 0;
	pArg->nArgLen = 0;
	pArg->bSymbol = false;
	pArg->eDevice = NUM_DEVICES; // none
	pArg->eToken  = NO_TOKEN   ; // none
	pArg->bType   = TYPE_STRING;
	pArg->nValue  = 0;
#if DEBUG_VAL_2
	pArg->nVal2   = 0;
#endif
}

//===========================================================================
void ArgsClear ()
{
	Arg_t *pArg = &g_aArgs[0];
	Arg_t *pRaw = &g_aArgRaw[0];

	for (int iArg = 0; iArg < MAX_ARGS; iArg++ )
	{
		ClearArg( pArg );
		ClearArg( pRaw );

		pArg++;
		pRaw++;
	}
}

//===========================================================================
bool ArgsGetValue ( Arg_t *pArg, WORD * pAddressValue_, const int nBase )
{
	_ASSERT(pArg);
	if (pArg == NULL)
		return false;

	TCHAR *pSrc = & (pArg->sArg[ 0 ]);
	TCHAR *pEnd = NULL;

	if (pAddressValue_)
	{
		*pAddressValue_ = (WORD)(_tcstoul( pSrc, &pEnd, nBase) & _6502_MEM_END);
		return true;
	}

	return false;
}

//===========================================================================
bool ArgsGetImmediateValue ( Arg_t *pArg, WORD * pAddressValue_ )
{
	if (pArg && pAddressValue_)
	{
		if (pArg->eToken == TOKEN_HASH)
		{
			pArg++;
			return ArgsGetValue( pArg, pAddressValue_ );
		}
	}

	return false;
}

// Read console input, process the raw args, turning them into tokens and types.
//===========================================================================
int	ArgsGet ( TCHAR * pInput )
{
	LPCTSTR pSrc = pInput;
	LPCTSTR pEnd = NULL;
	int     nBuf = 0;

	ArgToken_e iTokenSrc = NO_TOKEN;
	ArgToken_e iTokenEnd = NO_TOKEN;
	ArgType_e  iType     = TYPE_STRING;
	int     nLen;

	int     iArg = 0;
	int     nArg = 0;
	Arg_t  *pArg = &g_aArgRaw[0]; // &g_aArgs[0];

	g_pConsoleFirstArg = NULL;
					
	// BP FAC8:FACA // Range=3
	// BP FAC8,2    // Length=2
	// ^ ^^   ^^
	// | ||   |pSrc
	// | ||   pSrc
	// | |pSrc
	// | pEnd
	// pSrc
	while ((*pSrc) && (iArg < MAX_ARGS))
	{
		// Technically, there shouldn't be any leading spaces,
		// since pressing the spacebar is an alias for TRACE.
		// However, there is spaces between arguments
		pSrc = const_cast<char*>( SkipWhiteSpace( pSrc ));

		if (pSrc)
		{
			pEnd = FindTokenOrAlphaNumeric( pSrc, g_aTokens, NUM_TOKENS, &iTokenSrc );
			if ((iTokenSrc == NO_TOKEN) || (iTokenSrc == TOKEN_ALPHANUMERIC))
			{
				pEnd = SkipUntilToken( pSrc+1, g_aTokens, NUM_TOKENS, &iTokenEnd );
			}

			if (iTokenSrc == TOKEN_COMMENT_EOL)
				break; //pArg->eToken = iTokenSrc;
			
			if (iTokenSrc == NO_TOKEN)
			{
				iTokenSrc = TOKEN_ALPHANUMERIC;
			}

			iType = g_aTokens[ iTokenSrc ].eType;

			if (iTokenSrc == TOKEN_SEMI)
			{
				// TODO - command separator, must handle non-quoted though!
			}

			if (iTokenSrc == TOKEN_QUOTE_DOUBLE)
			{
				pSrc++; // Don't store start of quote
				pEnd = SkipUntilChar( pSrc, CHAR_QUOTE_DOUBLE );
			}
			else
			if (iTokenSrc == TOKEN_QUOTE_SINGLE)
			{
				pSrc++; // Don't store start of quote
				pEnd = SkipUntilChar( pSrc, CHAR_QUOTE_SINGLE );
			}

			if (pEnd)
			{
				nBuf = pEnd - pSrc;
			}

			if (nBuf > 0)
			{
				// Does anyone actually "need" > 132 character output???
				// Technically, we are capped via ParseInput(), g_aArgs[ iArg ] = g_aArgRaw[ iArg ];
				//if (iTokenSrc == TOKEN_QUOTE_DOUBLE)
				//	nLen = nBuf;
				nLen = MIN( nBuf, MAX_ARG_LEN ); // NOTE: see Arg_t.sArg[] // GH#481
				_tcsncpy( pArg->sArg, pSrc, nLen );
				pArg->sArg[ nLen ] = 0;
				pArg->nArgLen      = nLen;
				pArg->eToken       = iTokenSrc;
				pArg->bType        = iType;

				if (iTokenSrc == TOKEN_QUOTE_DOUBLE)
				{
					pEnd++; 
				}
				else
				if (iTokenSrc == TOKEN_QUOTE_SINGLE)
				{
					if (nLen > 1)
					{
						// Technically, chars aren't allowed to be multi-char
						// But we've extended the syntax to allow the user
						// to input High-Bit Apple Text
					}
					pEnd++; 
				}

				pSrc = pEnd;
				iArg++;
				pArg++;

				if (iArg == 1)
				{
					g_pConsoleFirstArg = pSrc;
				}
			}
		}
	}

	if (iArg)
	{
		nArg = iArg - 1; // first arg is command
	}

	g_nArgRaw = iArg;

	return nArg;
}


//===========================================================================
bool ArgsGetRegisterValue ( Arg_t *pArg, WORD * pAddressValue_ )
{
	bool bStatus = false;

	if (pArg && pAddressValue_)
	{
		// Check if we refer to reg A X Y P S
		for ( int iReg = 0; iReg < (NUM_BREAKPOINT_SOURCES-1); iReg++ )
		{
			// Skip Opcode/Instruction/Mnemonic
			if (iReg == BP_SRC_OPCODE)
				continue;

			// Skip individual flag names
			if ((iReg >= BP_SRC_FLAG_C) && (iReg <= BP_SRC_FLAG_N))
				continue;

			// Handle one char names
			if ((pArg->nArgLen == 1) && (pArg->sArg[0] == g_aBreakpointSource[ iReg ][0]))
			{
				switch ( iReg )
				{
					case BP_SRC_REG_A : *pAddressValue_ = regs.a  & 0xFF; bStatus = true; break;
					case BP_SRC_REG_P : *pAddressValue_ = regs.ps & 0xFF; bStatus = true; break;
					case BP_SRC_REG_X : *pAddressValue_ = regs.x  & 0xFF; bStatus = true; break;
					case BP_SRC_REG_Y : *pAddressValue_ = regs.y  & 0xFF; bStatus = true; break;
					case BP_SRC_REG_S : *pAddressValue_ = regs.sp       ; bStatus = true; break;
					default:
						break;
				}
			}
			else
			if (iReg == BP_SRC_REG_PC)
			{
				if ((pArg->nArgLen == 2) && (_tcscmp( pArg->sArg, g_aBreakpointSource[ iReg ] ) == 0))
				{
					*pAddressValue_ = regs.pc       ; bStatus = true; break;
				}
			}
		}
	}
	return bStatus;
}


//===========================================================================
void ArgsRawParse ( void )
{
	const int BASE = 16; // hex
	TCHAR *pSrc  = NULL;
	TCHAR *pEnd  = NULL;

	int    iArg = 1;
	Arg_t *pArg = & g_aArgRaw[ iArg ];
	int    nArg = g_nArgRaw;

	WORD   nAddressArg;
	WORD   nAddressSymbol;
	WORD   nAddressValue;

	while (iArg <= nArg)
	{
		pSrc  = & (pArg->sArg[ 0 ]);

		nAddressArg = (WORD)(_tcstoul( pSrc, &pEnd, BASE) & _6502_MEM_END);
		nAddressValue = nAddressArg;

		bool bFound = false;
		if (! (pArg->bType & TYPE_NO_SYM))
		{
			bFound = FindAddressFromSymbol( pSrc, & nAddressSymbol );
			if (bFound)
			{
				nAddressValue = nAddressSymbol;
				pArg->bSymbol = true;
			}
		}

		if (! (pArg->bType & TYPE_VALUE)) // already up to date?
			pArg->nValue = nAddressValue;

		pArg->bType |= TYPE_ADDRESS;

		iArg++;
		pArg++;
	}
}


/**
	@param nArgs         Number of raw args.

	Note: The number of args can be changed via:

		address1,length    Length
		address1:address2  Range
		address1+delta     Delta
		address1-delta     Delta
//=========================================================================== */
int ArgsCook ( const int nArgs )
{
	const int BASE = 16; // hex
	TCHAR *pSrc  = NULL;
	TCHAR *pEnd2 = NULL;

	int    nArg = nArgs;
	int    iArg = 1;
	Arg_t *pArg = NULL; 
	Arg_t *pPrev = NULL;
	Arg_t *pNext = NULL;

	WORD   nAddressArg;
	WORD   nAddressRHS;
	WORD   nAddressSym;
	WORD   nAddressVal;
	int    nParamLen = 0;
	int    nArgsLeft = 0;

	int nParenL = 0;
	int nParenR = 0;

	while (iArg <= nArg)
	{
		pArg  = & (g_aArgs[ iArg ]);
		pSrc  = & (pArg->sArg[ 0 ]);

		if (pArg->eToken == TOKEN_DOLLAR) // address
		{
// TODO: Need to flag was a DOLLAR token for assembler
			pNext = NULL;

			nArgsLeft = (nArg - iArg);
			if (nArgsLeft > 0)
			{
				pNext = pArg + 1;

				_Arg_Shift( iArg + 1, nArgs, iArg );
				nArg--;
				iArg--; // inc for start of next loop

				// Don't do register lookup
				pArg->bType |= TYPE_NO_REG;
			}
			else
				return ARG_SYNTAX_ERROR;
		}

		if (pArg->bType & TYPE_OPERATOR) // prev op type == address?
		{
			pPrev = NULL; // pLHS
			pNext = NULL; // pRHS
			nParamLen = 0;

			if (pArg->eToken == TOKEN_HASH) // HASH    # immediate
				nParamLen = 1;

			nArgsLeft = (nArg - iArg);
			if (nArgsLeft < nParamLen)
			{
				return ARG_SYNTAX_ERROR;
			}

			pPrev = pArg - 1;

			// Pass wildstar '*' to commands if only arg
			if ((pArg->eToken == TOKEN_STAR) && (nArg == 1))
				;
			else			
			if (nArgsLeft > 0) // These ops take at least 1 argument
			{
				pNext = pArg + 1;
				pSrc = &pNext->sArg[0];

				nAddressVal = 0;
				if (ArgsGetValue( pNext, & nAddressRHS ))
					nAddressVal = nAddressRHS;

				bool bFound = FindAddressFromSymbol( pSrc, & nAddressSym );
				if (bFound)
				{
					nAddressVal = nAddressSym;
					pArg->bSymbol = true;
				}

				// Comma and Colon are range operators, but they are not parsed here,
				// since args no longer have a 1st and 2nd value
/*
					pPrev->eToken = TOKEN_COLON;
					pPrev->bType |= TYPE_ADDRESS;
					pPrev->bType |= TYPE_RANGE;
*/

				if (pArg->eToken == TOKEN_AMPERSAND) // AND   & delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						  ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nValue &= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}								

				if (pArg->eToken == TOKEN_PIPE) // OR   | delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						  ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nValue |= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}								

				if (pArg->eToken == TOKEN_CARET) // XOR   ^ delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nValue ^= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_PLUS) // PLUS   + delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nValue += nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_MINUS) // MINUS  - delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nValue -= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_PERCENT) // PERCENT % delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nValue %= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_STAR) // STAR   * delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						  ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nValue *= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_FSLASH) // FORWARD SLASH / delta
				{
					if (pNext->eToken == TOKEN_FSLASH) // Comment
					{					
						nArg = iArg - 1;
						return nArg;
					}
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					if (! nAddressRHS)
						nAddressRHS = 1; // divide by zero bug
					pPrev->nValue /= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_EQUAL) // EQUAL  = assign
				{
					pPrev->nValue = nAddressRHS; 
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 0; // need token for Smart BreakPoints
				}					

				if (pArg->eToken == TOKEN_AT) // AT @ pointer de-reference
				{
					nParamLen = 1;
					_Arg_Shift( iArg + nParamLen, nArgs, iArg );
					nArg--;

					pArg->nValue   = 0; // nAddressRHS;
					pArg->bSymbol = false;

					int nPointers = g_vMemorySearchResults.size();
					if ((nPointers) &&
						(nAddressRHS < nPointers))
					{
						pArg->nValue   = g_vMemorySearchResults.at( nAddressRHS );
						pArg->bType   = TYPE_VALUE | TYPE_ADDRESS | TYPE_NO_REG | TYPE_NO_SYM;
					}
					nParamLen = 0;
				}
				
				if (pArg->eToken == TOKEN_HASH) // HASH    # immediate
				{
					pArg->nValue   = nAddressRHS;
					pArg->bSymbol = false;
					pArg->bType   = TYPE_VALUE | TYPE_ADDRESS | TYPE_NO_REG | TYPE_NO_SYM;
					nParamLen = 0;
				}

				if (pArg->eToken == TOKEN_LESS_THAN) // <
				{
					nParamLen = 0;
				}

				if (pArg->eToken == TOKEN_GREATER_THAN) // >
				{
					nParamLen = 0;
				}

				if (pArg->eToken == TOKEN_EXCLAMATION) // NOT !
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						if (! ArgsGetRegisterValue( pNext, & nAddressRHS ))
						{
							nAddressRHS = nAddressVal;
						}
					}
					pArg->nValue = ~nAddressRHS;
					pArg->bType |= TYPE_VALUE; // signal already up to date
					// Don't remove, since "SYM ! symbol" needs token to remove symbol
				}
				
				if (pArg->eToken == TOKEN_PAREN_L)
				{
					nParenL++;

					if (nArgsLeft >= 2)
					{
						nParamLen = 1; // eat '('
						_Arg_Shift( iArg + nParamLen, nArgs, iArg );

						pNext = & (g_aArgs[ iArg + 1 ]);
						if (pNext->eToken == TOKEN_PAREN_R)
						{
							nParenR++;
							pArg->bSymbol = false;

							// This is static binding, instead of dynamic binding
							// i.e. memdump (BRKV) -> memdump (3F0), but if 3F0 changes later
							// the debugger won't know that it has
							//	TODO: TYPE_INDIRECT
							// pArg->bType |= TYPE_INDIRECT;
							// pArg->nValue  =  nAddressVal;
							//nAddressVal = pNext->nValue;
							pArg->nValue  =  * (WORD*) (mem + nAddressVal);
							pArg->bType   = TYPE_VALUE | TYPE_ADDRESS | TYPE_NO_REG;

							iArg++; // eat ')'
							nArg -= 2;
							nParamLen = 0;
						}
						else
							return ARG_SYNTAX_ERROR; // ERROR: unbalanced/unmatched ( )
					}
				}							

				if (pArg->eToken == TOKEN_PAREN_R)
				{
					nParenR++;
					if (nParenL == nParenR)
					{
						nParamLen = 1;
					}
					else
						return ARG_SYNTAX_ERROR;
				}

				if (nParamLen)
				{
					_Arg_Shift( iArg + nParamLen, nArgs, iArg );
					nArg -= nParamLen;
					iArg = 0; // reset args, to handle multiple operators
				}
			}
			else
				return ARG_SYNTAX_ERROR;
		}
		else // not an operator, try (1) address, (2) symbol lookup
		{
			nAddressArg = (WORD)(_tcstoul( pSrc, &pEnd2, BASE) & _6502_MEM_END);

			if (! (pArg->bType & TYPE_NO_REG))
			{
				ArgsGetRegisterValue( pArg, & nAddressArg );
			}

			nAddressVal = nAddressArg;

			bool bFound = false;
			if (! (pArg->bType & TYPE_NO_SYM))
			{
				bFound = FindAddressFromSymbol( pSrc, & nAddressSym );
				if (bFound)
				{
					nAddressVal = nAddressSym;
					pArg->bSymbol = true;
				}
			}

			if (! (pArg->bType & TYPE_VALUE)) // already up to date?
				pArg->nValue = nAddressVal;

			pArg->bType |= TYPE_ADDRESS;
		}

		iArg++;
	}

	return nArg;
}


// Text Util ______________________________________________________________________________________



//===========================================================================
const char * ParserFindToken( const char *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e * pToken_ )
{
	if (! pSrc)
		return NULL;

	const TCHAR        *pName  = NULL;
	int   iToken;

	// Look-ahead for <=
	// Look-ahead for >=
	for (iToken = _TOKEN_FLAG_MULTI; iToken < NUM_TOKENS; iToken++ )
	{
		pName = & (g_aTokens[ iToken ].sToken[0]);
		if ((pSrc[0] == pName[ 0 ]) &&
			(pSrc[1] == pName[ 1 ]))
		{
			*pToken_ = g_aTokens[ iToken ].eToken;
			return pSrc + 2;
		}
	}

	const TokenTable_t *pToken = aTokens;

	for (iToken = 0; iToken < _TOKEN_FLAG_MULTI; iToken++ )
	{
		pName = & (pToken->sToken[0]);
		if (*pSrc == *pName)
		{
			if ( pToken_ )
			{
				*pToken_ = (ArgToken_e) iToken;
			}
			return pSrc + 1;
		}
		pToken++;
	}
	return NULL;
}


//===========================================================================
const TCHAR * FindTokenOrAlphaNumeric ( const TCHAR *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e * pToken_ )
{
	if ( pToken_ )
		*pToken_ = NO_TOKEN;

	const TCHAR *pEnd = pSrc;

	if (pSrc && (*pSrc))
	{
		if (isalnum( *pSrc ))
		{
			if (pToken_)
				*pToken_ = TOKEN_ALPHANUMERIC;
		}			
		else
		{
			pEnd = ParserFindToken( pSrc, aTokens, nTokens, pToken_ );
			if (! pEnd)
				pEnd = pSrc;
		}
	}
	return pEnd;
}


//===========================================================================
void TextConvertTabsToSpaces( TCHAR *pDeTabified_, LPCTSTR pText, const int nDstSize, int nTabStop )
{
	int TAB_SPACING = 8;
	int TAB_SPACING_1 = 16;
	int TAB_SPACING_2 = 21;

	if (nTabStop)
		TAB_SPACING = nTabStop;

	LPCTSTR pSrc = pText;
	LPTSTR  pDst = pDeTabified_;

	int nTab = 0; // gap left to next tab
	int nGap = 0; // actual gap
	int nCur = 0; // current cursor position
	while (pSrc && *pSrc && (nCur < nDstSize))
	{
		if (*pSrc == CHAR_TAB)
		{
			if (nTabStop)
			{
				nTab = nCur % TAB_SPACING;
				nGap = (TAB_SPACING - nTab);
			}
			else
			{
				if (nCur <= TAB_SPACING_1)
				{
					nGap = (TAB_SPACING_1 - nCur);
				}
				else
				if (nCur <= TAB_SPACING_2)
				{
					nGap = (TAB_SPACING_2 - nCur);
				}
				else
				{
					nTab = nCur % TAB_SPACING;
					nGap = (TAB_SPACING - nTab);
				}
			}
			

			if ((nCur + nGap) >= nDstSize)
				break;

			for ( int iSpc = 0; iSpc < nGap; iSpc++ )
			{
				*pDst++ = CHAR_SPACE;
			}
			nCur += nGap;
		}
		else
		if ((*pSrc == CHAR_LF) || (*pSrc == CHAR_CR))
		{
			*pDst++ = 0; // *pSrc;
			nCur++;
		}
		else
		{
			*pDst++ = *pSrc;
			nCur++;
		}
		pSrc++;
	}	
	*pDst = 0;
}


// @return Length of new string
//===========================================================================
int RemoveWhiteSpaceReverse ( TCHAR *pSrc )
{
	int   nLen = _tcslen( pSrc );
	char *pDst = pSrc + nLen;
	while (nLen--)
	{
		pDst--;
		if (*pDst == CHAR_SPACE)
		{
			*pDst = 0;
		}
		else
		{
			break;
		}
	}
	return nLen;
}
