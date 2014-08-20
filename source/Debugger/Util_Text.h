#pragma once

#define CHAR_LF           '\x0D'
#define CHAR_CR           '\x0A'
#define CHAR_SPACE        ' '
#define CHAR_TAB          '\t'
#define CHAR_QUOTE_DOUBLE '"' 
#define CHAR_QUOTE_SINGLE '\''
#define CHAR_ESCAPE       '\x1B'

// Text Util

inline	const char* EatEOL                    ( const char *pSrc )
		{
			if (pSrc)
			{
				if (*pSrc == CHAR_LF)
					pSrc++;

				if (*pSrc == CHAR_CR)
					pSrc++;
			}
			return pSrc;
		}

inline	const char* SkipWhiteSpace             ( const char *pSrc )
		{
			while (pSrc && ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB)))
			{
				pSrc++;
			}
			return pSrc;
		}

inline	const char* SkipWhiteSpaceReverse      ( const char *pSrc, const char *pStart )
		{
			while (pSrc && ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB)) && (pSrc > pStart))
			{
				pSrc--;
			}
			return pSrc;
		}

inline	const char* SkipUntilChar              ( const char *pSrc, const char nDelim )
		{
			while (pSrc && (*pSrc))
			{
				if (*pSrc == nDelim)
					break;
				pSrc++;
			}
			return pSrc;
		}

inline	const char* SkipUntilEOL               ( const char *pSrc )
		{
			// EOL delims: NULL, LF, CR
			while (pSrc && (*pSrc))
			{
				if ((*pSrc == CHAR_LF) || (*pSrc == CHAR_CR))
				{
					break;
				}
				pSrc++;
			}
			return pSrc;
		}

inline	const char* SkipUntilTab               ( const char *pSrc)
		{
			while (pSrc && (*pSrc))
			{
				if (*pSrc == CHAR_TAB)
				{
					break;
				}
				pSrc++;
			}
			return pSrc;
		}

inline	const char* SkipUntilWhiteSpace        ( const char *pSrc )
		{
			while (pSrc && (*pSrc))
			{
				if ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB))
				{
					break;
				}
				pSrc++;
			}
			return pSrc;
		}

inline	const char* SkipUntilWhiteSpaceReverse ( const char *pSrc, const char *pStart )
		{
			while (pSrc && (pSrc > pStart))
			{
				if ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB))
				{
					break;
				}
				pSrc--;
			}
			return pSrc;
		}



/** Assumes text are valid hex digits!
//=========================================================================== */
inline	BYTE TextConvert2CharsToByte ( char *pText )
		{
			BYTE n = ((pText[0] <= '@') ? (pText[0] - '0') : (pText[0] - 'A' + 10)) << 4;
				n += ((pText[1] <= '@') ? (pText[1] - '0') : (pText[1] - 'A' + 10)) << 0;
			return n;
		}

//===========================================================================
inline	bool TextIsHexChar( char nChar )
		{
			if ((nChar >= '0') && (nChar <= '9'))
				return true;

			if ((nChar >= 'A') && (nChar <= 'F'))
				return true;

			if ((nChar >= 'a') && (nChar <= 'f'))
				return true;

			return false;
		}


//===========================================================================
inline	bool TextIsHexByte( char *pText )
		{
			if (TextIsHexChar( pText[0] ) && 
				TextIsHexChar( pText[1] ))
				return true;

			return false;
		}

//===========================================================================
inline	bool TextIsHexString ( LPCSTR pText )
	{
		while (*pText)
		{
			if (! TextIsHexChar( *pText ))
				return false;

			pText++;
		}
		return true;
	}
