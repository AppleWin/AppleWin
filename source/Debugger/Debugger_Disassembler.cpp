/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2021, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

#include "Debug.h"

#include "../Memory.h"

inline static void _memsetz(void* dst, int val, size_t len)
{
	memset(dst, val, len);
	static_cast<char*>(dst)[len] = 0;
}

//===========================================================================
std::string FormatAddress(WORD nAddress, int nBytes)
{
	switch (nBytes)
	{
	case  2:	return StrFormat("$%02X", (unsigned)nAddress); break;
	case  3:	return StrFormat("$%04X", (unsigned)nAddress); break;
	// TODO: FIXME: Can we get called with nBytes == 16 ??
	default:	break; // clear since is static
	}
	// There is no symbol for this nAddress
	return std::string();
}

//===========================================================================
char* FormatCharCopy(char* pDst, const char* pEnd, const char* pSrc, const int nLen)
{
	for (int i = 0; i < nLen && pDst < pEnd; i++)
		*pDst++ = FormatCharTxtCtrl(*pSrc++);
	return pDst;
}

//===========================================================================
char  FormatCharTxtAsci(const BYTE b, bool* pWasAsci_)
{
	if (pWasAsci_)
		*pWasAsci_ = false;

	char c = (b & 0x7F);
	if (b <= 0x7F)
	{
		if (pWasAsci_)
		{
			*pWasAsci_ = true;
		}
	}
	return c;
}

// Note: FormatCharTxtCtrl() and RemapChar()
//===========================================================================
char  FormatCharTxtCtrl(const BYTE b, bool* pWasCtrl_)
{
	if (pWasCtrl_)
		*pWasCtrl_ = false;

	char c = (b & 0x7F); // .32 Changed: Lo now maps High Ascii to printable chars. i.e. ML1 D0D0
	if (b < 0x20) // SPACE
	{
		if (pWasCtrl_)
		{
			*pWasCtrl_ = true;
		}
		c = b + '@'; // map ctrl chars to visible
	}
	return c;
}

//===========================================================================
char  FormatCharTxtHigh(const BYTE b, bool* pWasHi_)
{
	if (pWasHi_)
		*pWasHi_ = false;

	char c = b;
	if (b > 0x7F)
	{
		if (pWasHi_)
		{
			*pWasHi_ = true;
		}
		c = (b & 0x7F);
	}
	return c;
}


//===========================================================================
char FormatChar4Font(const BYTE b, bool* pWasHi_, bool* pWasLo_)
{
	// Most Windows Fonts don't have (printable) glyphs for control chars
	BYTE b1 = FormatCharTxtHigh(b, pWasHi_);
	BYTE b2 = FormatCharTxtCtrl(b1, pWasLo_);
	return b2;
}



// Disassembly
	/*
		// Thought about moving MouseText to another location, say high bit, 'A' + 0x80
		// But would like to keep compatibility with existing CHARSET40 - UPDATE: we now use original video ROMs (GH#1308)
		// Since we should be able to display all apple chars 0x00 .. 0xFF with minimal processing
		// Use CONSOLE_COLOR_ESCAPE_CHAR to shift to mouse text
		* Apple Font
			K      Mouse Text Up Arror
			H      Mouse Text Left Arrow
			J      Mouse Text Down Arrow
		* Wingdings
			\xE1   Up Arrow
			\xE2   Down Arrow
		* Webdings // M$ Font
			\x35   Up Arrow
			\x33   Left Arrow  (\x71 recycl is too small to make out details)
			\x36   Down Arrow
		* Symols
			\xAD   Up Arrow
			\xAF   Down Arrow
		* ???
			\x18 Up
			\x19 Down
	*/
#if USE_APPLE_FONT
const char* g_sConfigBranchIndicatorUp[NUM_DISASM_BRANCH_TYPES + 1] = { " ", "^", "\x8B" }; // "`K" 0x4B
const char* g_sConfigBranchIndicatorEqual[NUM_DISASM_BRANCH_TYPES + 1] = { " ", "=", "\x88" }; // "`H" 0x48
const char* g_sConfigBranchIndicatorDown[NUM_DISASM_BRANCH_TYPES + 1] = { " ", "v", "\x8A" }; // "`J" 0x4A
#else
const char* g_sConfigBranchIndicatorUp[NUM_DISASM_BRANCH_TYPES + 1] = { " ", "^", "\x35" };
const char* g_sConfigBranchIndicatorEqual[NUM_DISASM_BRANCH_TYPES + 1] = { " ", "=", "\x33" };
char* g_sConfigBranchIndicatorDown[NUM_DISASM_BRANCH_TYPES + 1] = { " ", "v", "\x36" };
#endif

// Disassembly ____________________________________________________________________________________

void GetTargets_IgnoreDirectJSRJMP(const BYTE iOpcode, int& nTargetPointer)
{
	if (iOpcode == OPCODE_JSR || iOpcode == OPCODE_JMP_A)
		nTargetPointer = NO_6502_TARGET;
}


// Get the data needed to disassemble one line of opcodes. Fills in the DisasmLine info.
// Disassembly formatting flags returned
//	@parama sTargetValue_ indirect/indexed final value
//===========================================================================
int GetDisassemblyLine(WORD nBaseAddress, DisasmLine_t& line_)
//	char *sAddress_, char *sOpCodes_,
//	char *sTarget_, char *sTargetOffset_, int & nTargetOffset_,
//	char *sTargetPointer_, char *sTargetValue_,
//	char *sImmediate_, char & nImmediate_, char *sBranch_ )
{
	line_.Clear();

	int iOpcode;
	int iOpmode;
	int nOpbyte;

	iOpcode = _6502_GetOpmodeOpbyte(nBaseAddress, iOpmode, nOpbyte, &line_.pDisasmData);
	const DisasmData_t* pData = line_.pDisasmData; // Disassembly_IsDataAddress( nBaseAddress );

	line_.iOpcode = iOpcode;
	line_.iOpmode = iOpmode;
	line_.nOpbyte = nOpbyte;

#if _DEBUG
	//	if (iLine != 41)
	//		return nOpbytes;
#endif

	if (iOpmode == AM_M)
		line_.bTargetImmediate = true;

	if ((iOpmode >= AM_IZX) && (iOpmode <= AM_NA))
		line_.bTargetIndirect = true; // ()

	if ((iOpmode >= AM_IZX) && (iOpmode <= AM_NZY))
		line_.bTargetIndexed = true; // ()

	if (((iOpmode >= AM_A) && (iOpmode <= AM_ZY)) || line_.bTargetIndirect)
		line_.bTargetValue = true; // #$

	if ((iOpmode == AM_AX) || (iOpmode == AM_ZX) || (iOpmode == AM_IZX) || (iOpmode == AM_IAX))
		line_.bTargetX = true; // ,X

	if ((iOpmode == AM_AY) || (iOpmode == AM_ZY) || (iOpmode == AM_NZY))
		line_.bTargetY = true; // ,Y

	unsigned int nMinBytesLen = (DISASM_DISPLAY_MAX_OPCODES * (2 + g_bConfigDisasmOpcodeSpaces)); // 2 char for byte (or 3 with space)

	int bDisasmFormatFlags = 0;

	// Composite string that has the symbol or target nAddress
	WORD nTarget = 0;

	if ((iOpmode != AM_IMPLIED) &&
		(iOpmode != AM_1) &&
		(iOpmode != AM_2) &&
		(iOpmode != AM_3))
	{
		// Assume target address starts after the opcode ...
		// BUT in the Assembler Directive / Data Disassembler case for define addr/word
		// the opcode literally IS the target address!
		if (pData)
		{
			nTarget = pData->nTargetAddress;
		}
		else {
			nTarget = mem[(nBaseAddress + 1) & 0xFFFF] | (mem[(nBaseAddress + 2) & 0xFFFF] << 8);
			if (nOpbyte == 2)
				nTarget &= 0xFF;
		}

		if (iOpmode == AM_R) // Relative
		{
			line_.bTargetRelative = true;

			nTarget = nBaseAddress + 2 + (int)(signed char)nTarget;

			line_.nTarget = nTarget;
			strncpy_s(line_.sTargetValue, WordToHexStr(nTarget & 0xFFFF).c_str(), _TRUNCATE);

			// Always show branch indicators
			bDisasmFormatFlags |= DISASM_FORMAT_BRANCH;

			if (nTarget < nBaseAddress)
				strncpy_s(line_.sBranch, g_sConfigBranchIndicatorUp[g_iConfigDisasmBranchType], _TRUNCATE);
			else
			if (nTarget > nBaseAddress)
				strncpy_s(line_.sBranch, g_sConfigBranchIndicatorDown[g_iConfigDisasmBranchType], _TRUNCATE);
			else
				strncpy_s(line_.sBranch, g_sConfigBranchIndicatorEqual[g_iConfigDisasmBranchType], _TRUNCATE);

			bDisasmFormatFlags |= DISASM_FORMAT_TARGET_POINTER;
			if (g_iConfigDisasmTargets & DISASM_TARGET_ADDR)
				strncpy_s(line_.sTargetPointer, WordToHexStr(nTarget & 0xFFFF).c_str(), _TRUNCATE);
		}
		// intentional re-test AM_R ...

//		if ((iOpmode >= AM_A  ) && (iOpmode <= AM_NA))
		if ((iOpmode == AM_A  ) || // Absolute
			(iOpmode == AM_Z  ) || // Zeropage
			(iOpmode == AM_AX ) || // Absolute, X
			(iOpmode == AM_AY ) || // Absolute, Y
			(iOpmode == AM_ZX ) || // Zeropage, X
			(iOpmode == AM_ZY ) || // Zeropage, Y
			(iOpmode == AM_R  ) || // Relative
			(iOpmode == AM_IZX) || // Indexed (Zeropage Indirect, X)
			(iOpmode == AM_IAX) || // Indexed (Absolute Indirect, X)
			(iOpmode == AM_NZY) || // Indirect (Zeropage) Index, Y
			(iOpmode == AM_NZ ) || // Indirect (Zeropage)
			(iOpmode == AM_NA ))   //(Indirect Absolute)
		{
			line_.nTarget = nTarget;

			std::string const* pTarget = NULL;
			std::string const* pSymbol = FindSymbolFromAddress(nTarget, &line_.iTargetTable);
			std::string sAddressBuf;

			// Data Assembler
			if (pData && (!pData->bSymbolLookup))
				pSymbol = NULL;

			// Try exact match first
			if (pSymbol)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
				pTarget = pSymbol;
			}

			if (!(bDisasmFormatFlags & DISASM_FORMAT_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress(nTarget - 1, &line_.iTargetTable);
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
					bDisasmFormatFlags |= DISASM_FORMAT_OFFSET;
					pTarget = pSymbol;
					line_.nTargetOffset = +1; // U FA82   LDA $3F1 BREAK-1
				}
			}

			// Old Offset search: (Search +1 First) nTarget-1, (Search -1 Second) nTarget+1
			//    Problem: U D038 shows as A.TRACE+1
			// New Offset search: (Search -1 First) nTarget+1, (Search +1 Second) nTarget+1
			//    Problem: U D834, D87E shows as P.MUL-1, instead of P.ADD+1
			// 2.6.2.31 Fixed: address table was bailing on first possible match. U D000 -> da STOP+1, instead of END-1
			// 2.7.0.0: Try to match nTarget-1, nTarget+1, AND if we have both matches
			// Then we need to decide which one to show. If we have pData then pick this one.
			// TODO: Do we need to let the user decide which one they want searched first?
			//    nFirstTarget = g_bDebugConfig_DisasmMatchSymbolOffsetMinus1First ? nTarget-1 : nTarget+1;
			//    nSecondTarget = g_bDebugConfig_DisasmMatchSymbolOffsetMinus1First ? nTarget+1 : nTarget-1;
			if (!(bDisasmFormatFlags & DISASM_FORMAT_SYMBOL) || pData)
			{
				pSymbol = FindSymbolFromAddress(nTarget + 1,&line_.iTargetTable);
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
					bDisasmFormatFlags |= DISASM_FORMAT_OFFSET;
					pTarget = pSymbol;
					line_.nTargetOffset = -1; // U FA82 LDA $3F3 BREAK+1
				}
			}

			if (!(bDisasmFormatFlags & DISASM_FORMAT_SYMBOL))
			{
				sAddressBuf = FormatAddress(nTarget, (iOpmode != AM_R) ? nOpbyte : 3);	// GH#587: For Bcc opcodes, pretend it's a 3-byte opcode to print a 16-bit target addr
				pTarget = &sAddressBuf;
			}

			//sTarget = StrFormat( g_aOpmodes[ iOpmode ].m_sFormat, pTarget->c_str() );
			if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
			{
				int nAbsTargetOffset = (line_.nTargetOffset > 0) ? line_.nTargetOffset : -line_.nTargetOffset;
				strncpy_s(line_.sTargetOffset, StrFormat("%d", nAbsTargetOffset).c_str(), _TRUNCATE);
			}
			strncpy_s(line_.sTarget, pTarget->c_str(), _TRUNCATE);

			// Indirect / Indexed
			int nTargetPartial;
			int nTargetPartial2;
			int nTargetPointer;
			WORD nTargetValue = 0; // de-ref
			_6502_GetTargets(nBaseAddress, &nTargetPartial, &nTargetPartial2, &nTargetPointer, NULL);
			GetTargets_IgnoreDirectJSRJMP(iOpcode, nTargetPointer);	// For *direct* JSR/JMP, don't show 'addr16:byte char'

			if (nTargetPointer != NO_6502_TARGET)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_TARGET_POINTER;

				nTargetValue = *(mem + nTargetPointer) | (*(mem + ((nTargetPointer + 1) & 0xffff)) << 8);

				//if (((iOpmode >= AM_A) && (iOpmode <= AM_NZ)) && (iOpmode != AM_R))
				//	sTargetValue_ = WordToHexStr( nTargetValue ); // & 0xFFFF

				if (g_iConfigDisasmTargets & DISASM_TARGET_ADDR)
					strncpy_s(line_.sTargetPointer, WordToHexStr(nTargetPointer & 0xFFFF).c_str(), _TRUNCATE);

				if (iOpcode != OPCODE_JMP_NA && iOpcode != OPCODE_JMP_IAX)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_TARGET_VALUE;
					if (g_iConfigDisasmTargets & DISASM_TARGET_VAL)
						strncpy_s(line_.sTargetValue, ByteToHexStr(nTargetValue & 0xFF).c_str(), _TRUNCATE);

					bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
					line_.nImmediate = (BYTE)nTargetValue;

					const char _char = FormatCharTxtCtrl(FormatCharTxtHigh(line_.nImmediate, NULL), NULL);
					_memsetz(line_.sImmediate, _char, 1);

					//if (ConsoleColorIsEscapeMeta( nImmediate_ ))
#if OLD_CONSOLE_COLOR
					if (ConsoleColorIsEscapeMeta(_char))
						_memsetz(line_.sImmediate, _char, 2);
					else
						_memsetz(line_.sImmediate, _char, 1);
#endif
				}

				//if (iOpmode == AM_NA ) // Indirect Absolute
				//	sTargetValue_ = WordToHexStr( nTargetPointer & 0xFFFF );
				//else
				//	//sTargetValue_ = ByteToHexStr( nTargetValue & 0xFF );
				//	sTargetValue_ = StrFormat( "%04X:%02X", nTargetPointer & 0xFFFF, nTargetValue & 0xFF );
			}
		}
		else
		{
			if (iOpmode == AM_M)
			{
				strncpy_s(line_.sTarget, ByteToHexStr((BYTE)nTarget).c_str(), _TRUNCATE);

				if (nTarget == 0)
					line_.sImmediateSignedDec[0] = 0; // nothing
				else
				if (nTarget < 128)
					strncpy_s(line_.sImmediateSignedDec, StrFormat("+%d", nTarget).c_str(), _TRUNCATE);
				else
				if (nTarget >= 128)
					strncpy_s(line_.sImmediateSignedDec, StrFormat("-%d" , (~nTarget + 1) & 0xFF).c_str(), _TRUNCATE);

				bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
				line_.nImmediate = (BYTE)nTarget;

				const char _char = FormatCharTxtCtrl(FormatCharTxtHigh(line_.nImmediate, NULL), NULL);
				_memsetz(line_.sImmediate, _char, 1);

#if OLD_CONSOLE_COLOR
				if (ConsoleColorIsEscapeMeta(_char))
					_memsetz(line_.sImmediate, _char, 2);
				else
					_memsetz(line_.sImmediate, _char, 1);
#endif
			}
		}
	}

	strncpy_s(line_.sAddress, WordToHexStr(nBaseAddress).c_str(), _TRUNCATE);

	// Opcode Bytes
	FormatOpcodeBytes(nBaseAddress, line_);

	// Data Disassembler
	if (pData)
	{
		line_.iNoptype = pData->eElementType;
		line_.iNopcode = pData->iDirective;
		strcpy(line_.sMnemonic, g_aAssemblerDirectives[line_.iNopcode].m_pMnemonic);

		FormatNopcodeBytes(nBaseAddress, line_);
	}
	else { // Regular 6502/65C02 opcode -> mnemonic
		strcpy(line_.sMnemonic, g_aOpcodes[line_.iOpcode].sMnemonic);
	}

	const size_t nOpCodesLen = strlen(line_.sOpCodes);
	if (nOpCodesLen < nMinBytesLen)
	{
		memset(line_.sOpCodes + nOpCodesLen, ' ', nMinBytesLen - nOpCodesLen);
		line_.sOpCodes[nMinBytesLen] = '\0';
	}

	return bDisasmFormatFlags;
}

//===========================================================================
void FormatOpcodeBytes(WORD nBaseAddress, DisasmLine_t& line_)
{
	// 2.8.0.0 fix // TODO: FIX: show max 8 bytes for HEX
	const int nMaxOpBytes = std::min<int>(line_.nOpbyte, DISASM_DISPLAY_MAX_OPCODES);

	char*             cp = line_.sOpCodes;
	const char* const ep = cp + sizeof(line_.sOpCodes);
	for (int iByte = 0; iByte < nMaxOpBytes; iByte++)
	{
		const BYTE nMem = mem[(nBaseAddress + iByte) & 0xFFFF];
		if ((cp+2) < ep)
			cp = StrBufferAppendByteAsHex(cp, nMem);

		// TODO: If Disassembly_IsDataAddress() don't show spaces...
		if (g_bConfigDisasmOpcodeSpaces)
		{
			if ((cp+1) < ep)
				*cp++ = ' ';
		}
	}
	*cp = '\0';
}

struct FAC_t
{
	uint8_t  negative;
	 int8_t  exponent;
	uint32_t mantissa;

	bool     isZero;
};

void FAC_Unpack(WORD nAddress, FAC_t& fac_)
{
	BYTE e0 = *(LPBYTE)(mem + nAddress + 0);
	BYTE m1 = *(LPBYTE)(mem + nAddress + 1);
	BYTE m2 = *(LPBYTE)(mem + nAddress + 2);
	BYTE m3 = *(LPBYTE)(mem + nAddress + 3);
	BYTE m4 = *(LPBYTE)(mem + nAddress + 4);

	// sign
	//     EB82:A5 9D       SIGN  LDA FAC
	//     EB84:F0 09             BEQ SIGN3     ; zero
	//     EB86:A5 A2       SIGN1 LDA FAC.SIGN
	//     EB88:2A          SIGN2 ROL
	//     EB89:A9 FF             LDA #$FF      ; negative
	//     EB8B:B0 02             BCS SIGN3
	//     EB8D:A9 01             LDA #$01      ; positive
	//     EB8F:60          SIGN3
	
	fac_.exponent = e0 - 0x80;
	fac_.negative =(m1 & 0x80) >> 7;	// EBAF:46 A2       ABS   LSR FAC.SIGN
	fac_.mantissa = 0
		| ((m1 | 0x80) << 24) // implicit 1.0, EB12: ORA #$80, STA FAC+1
		| ((m2       ) << 16)
		| ((m3       ) <<  8)
		| ((m4       ) <<  0);

	fac_.isZero = (e0 == 0); // TODO: need to check mantissa?
}


// Formats Target string with bytes,words, string, etc...
//===========================================================================
void FormatNopcodeBytes(WORD nBaseAddress, DisasmLine_t& line_)
{
	// TODO: One day, line_.sTarget should become a std::string and things would be much simpler.
	char*             pDst          = line_.sTarget;
	const char* const pEnd          = pDst + sizeof(line_.sTarget);
	const uint32_t    nStartAddress = line_.pDisasmData->nStartAddress;
	const uint32_t    nEndAddress   = line_.pDisasmData->nEndAddress;
	const int         nDisplayLen   = nEndAddress - nBaseAddress + 1; // *inclusive* KEEP IN SYNC: _CmdDefineByteRange() CmdDisasmDataList() _6502_GetOpmodeOpbyte() FormatNopcodeBytes()

	for (int iByte = 0; iByte < line_.nOpbyte; )
	{
		BYTE nTarget8  = *(LPBYTE)(mem + nBaseAddress + iByte);
		WORD nTarget16 = *(LPWORD)(mem + nBaseAddress + iByte);

		switch (line_.iNoptype)
		{
			case NOP_BYTE_1:
			case NOP_BYTE_2:
			case NOP_BYTE_4:
			case NOP_BYTE_8:
				if ((pDst + 2) < pEnd)
					pDst = StrBufferAppendByteAsHex(pDst, nTarget8);
				iByte++;
				if (line_.iNoptype == NOP_BYTE_1)
				{
					if (iByte < line_.nOpbyte)
					{
						if ((pDst + 1) < pEnd)
							*pDst++ = ',';
					}
				}
				*pDst = '\0';
				break;

			case NOP_FAC:
			{
				FAC_t fac;
				FAC_Unpack( nBaseAddress, fac );
				const char aSign[2] = { '+', '-' };
				if (fac.isZero)
				{
					// 2.9.1.21 Fixed: `df` showing zero was displaying 0 instead 0.0
					// Format 0.0 so users know this is a floating point value
					std::string sFac( "0.0" );
					if ((pDst + 3) < pEnd)
					{
						memcpy(pDst, sFac.c_str(), sFac.length());
						pDst += sFac.length();
					}
					// No room???
				}
				else
				{
					const double f    = fac.mantissa * pow( 2.0, fac.exponent - 32 );
					//std::string sFac = StrFormat( "s%1X m%08X e%02X", fac.negative, fac.mantissa, fac.exponent & 0xFF );
					// 2.9.1.23: Show floating-point values in scientific notation.
					std::string  sFac = StrFormat( "%c%e", aSign[ fac.negative ], f );
					if ((pDst + sFac.length()) < pEnd)
					{
						memcpy(pDst, sFac.c_str(), sFac.length());
						pDst += sFac.length();
					}
				}
				iByte += 5;
				*pDst = '\0';
				break;
			}

			case NOP_WORD_1:
			case NOP_WORD_2:
			case NOP_WORD_4:
				if ((pDst + 4) < pEnd)
					pDst = StrBufferAppendWordAsHex(pDst, nTarget16);
				iByte += 2;
				if (iByte < line_.nOpbyte)
				{
					if ((pDst + 1) < pEnd)
						*pDst++ = ',';
				}
				*pDst = '\0';
				break;

			case NOP_ADDRESS:
				// Nothing to do, already handled :-)
				iByte += 2;
				break;

			case NOP_STRING_APPLESOFT:
				iByte = line_.nOpbyte;
				if ((pDst + iByte) < pEnd)
				{
					memcpy(pDst, mem + nBaseAddress, iByte);
					pDst += iByte;
				}
				*pDst = 0;
				break;

			case NOP_STRING_APPLE:
			{
				iByte = line_.nOpbyte; // handle all bytes of text
				const char* pSrc = (const char*)mem + nStartAddress;

				if (nDisplayLen > (DISASM_DISPLAY_MAX_IMMEDIATE_LEN - 2)) // does "text" fit?
				{
					const bool ellipsis = (nDisplayLen > DISASM_DISPLAY_MAX_IMMEDIATE_LEN);
					const int len = (ellipsis) ? (DISASM_DISPLAY_MAX_IMMEDIATE_LEN - 3)
											   : nDisplayLen // no need of extra characters for ellipsis
											   ;

					// DISPLAY: text_longer_18...
					pDst = FormatCharCopy(pDst, pEnd, pSrc, len); // BUG: #251 v2.8.0.7: ASC #:# with null byte doesn't mark up properly

					if (ellipsis && (pDst + 3) < pEnd)
					{
						*pDst++ = '.';
						*pDst++ = '.';
						*pDst++ = '.';
					}
				}
				else
				{ // DISPLAY: "max_18_char"
					if ((pDst + 1) < pEnd)
						*pDst++ = '"';
					pDst = FormatCharCopy(pDst, pEnd, pSrc, nDisplayLen); // BUG: #251 v2.8.0.7: ASC #:# with null byte doesn't mark up properly
					if ((pDst + 1) < pEnd)
						*pDst++ = '"';
				}

				*pDst = 0;
				break;
			}

			default:
#if _DEBUG // Unhandled data disassembly!
				assert(false);
#endif
				iByte++;
				break;
		} // switch.
	} // for.
}

//===========================================================================
std::string FormatDisassemblyLine(const DisasmLine_t& line)
{
	//> Address Separator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//
	// Data Disassembler
	//                              Label Directive       [Immediate]

	std::string sDisassembly = StrFormat( "%s:%s %s "
		, line.sAddress
		, line.sOpCodes
		, g_aOpcodes[line.iOpcode].sMnemonic
	);

	/*
	if (line.bTargetIndexed || line.bTargetIndirect)
		sDisassembly += '(';

	if (line.bTargetImmediate)
		sDisassembly += "#$";

	if (line.bTargetValue)
		sDisassembly += line.sTarget;

	if (line.bTargetIndirect)
	{
		if (line.bTargetX)
			sDisassembly += ", X";
		if (line.bTargetY)
			sDisassembly += ", Y";
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
		sDisassembly += ')';

	if (line.bTargetIndirect)
	{
		if (line.bTargetY)
			sDisassembly += ", Y";
	}
	*/

	if (line.bTargetValue || line.bTargetRelative || line.bTargetImmediate)
	{
		if (line.bTargetRelative)
		{
			sDisassembly += '$';
			sDisassembly += line.sTargetValue;
		}
		else
		{
			if (line.bTargetImmediate)
			{
				sDisassembly += "#$";
				sDisassembly += line.sTarget;
			}
			else
			{
				sDisassembly += '$';
				sDisassembly += StrFormat( g_aOpmodes[line.iOpmode].m_sFormat, line.nTarget );
			}
		}
	}

	return sDisassembly;
}

// Given an Address, and Line to display it on
// Calculate the address of the top and bottom lines
// @param bUpdateCur
// true  = Update Cur based on Top
// false = Update Top & Bot based on Cur
//===========================================================================
void DisasmCalcTopFromCurAddress(bool bUpdateTop)
{
	int nLen = g_nDisasmCurLine * 3; // max 3 opcodes/instruction, is our search window

	// Look for a start address that when disassembled,
	// will have the cursor on the specified line and address
	int iTop = g_nDisasmCurAddress - nLen;
	int iCur = g_nDisasmCurAddress;

	g_bDisasmCurBad = false;

	bool bFound = false;
	while (iTop <= iCur)
	{
		WORD iAddress = iTop;
		//		int iOpcode;
		int iOpmode;
		int nOpbytes;

		for (int iLine = 0; iLine <= nLen; iLine++) // min 1 opcode/instruction
		{
			// a.
			_6502_GetOpmodeOpbyte(iAddress, iOpmode, nOpbytes);
			// b.
			//			_6502_GetOpcodeOpmodeOpbyte( iOpcode, iOpmode, nOpbytes );

			if (iLine == g_nDisasmCurLine) // && (iAddress == g_nDisasmCurAddress))
			{
				if (iAddress == g_nDisasmCurAddress)
					// b.
					//					&& (iOpmode != AM_1) &&
					//					&& (iOpmode != AM_2) &&
					//					&& (iOpmode != AM_3) &&
					//					&& _6502_IsOpcodeValid( iOpcode))
				{
					g_nDisasmTopAddress = iTop;
					bFound = true;
					break;
				}
			}

			// .20 Fixed: DisasmCalcTopFromCurAddress()
			//if ((eMode >= AM_1) && (eMode <= AM_3))
#if 0 // _DEBUG
			LogOutput("%04X : %d bytes\n", iAddress, nOpbytes);
#endif
			iAddress += nOpbytes;
		}
		if (bFound)
		{
			break;
		}
		iTop++;
	}

	if (!bFound)
	{
		// Well, we're up the creek.
		// There is no (valid) solution!
		// Basically, there is no address, that when disassembled,
		// will put our Address on the cursor Line!
		// So, like typical game programming, when we don't like the solution, change the problem!
//		if (bUpdateTop)
		g_nDisasmTopAddress = g_nDisasmCurAddress;

		g_bDisasmCurBad = true; // Bad Disassembler, no opcode for you!

		// We reall should move the cursor line to the top for one instruction.
		// Moving the cursor line around is not really a good idea, since we're breaking consistency paradigm for the user.
		// g_nDisasmCurLine = 0;
#if 0 // _DEBUG
		std::string sText = StrFormat("DisasmCalcTopFromCurAddress()\n"
										"\tTop: %04X\n"
										"\tLen: %04X\n"
										"\tMissed: %04X",
			g_nDisasmCurAddress - nLen, nLen, g_nDisasmCurAddress);
		GetFrame().FrameMessageBox(sText.c_str(), "ERROR", MB_OK);
#endif
	}
}


//===========================================================================
WORD DisasmCalcAddressFromLines(WORD iAddress, int nLines)
{
	while (nLines-- > 0)
	{
		int iOpmode;
		int nOpbytes;
		_6502_GetOpmodeOpbyte(iAddress, iOpmode, nOpbytes);
		iAddress += nOpbytes;
	}
	return iAddress;
}


//===========================================================================
void DisasmCalcCurFromTopAddress()
{
	g_nDisasmCurAddress = DisasmCalcAddressFromLines(g_nDisasmTopAddress, g_nDisasmCurLine);
}


//===========================================================================
void DisasmCalcBotFromTopAddress()
{
	g_nDisasmBotAddress = DisasmCalcAddressFromLines(g_nDisasmTopAddress, g_nDisasmWinHeight);
}


//===========================================================================
void DisasmCalcTopBotAddress()
{
	DisasmCalcTopFromCurAddress();
	DisasmCalcBotFromTopAddress();
}
