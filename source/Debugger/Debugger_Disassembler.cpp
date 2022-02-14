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

//===========================================================================
const char* FormatAddress(WORD nAddress, int nBytes)
{
	// There is no symbol for this nAddress
	static TCHAR sSymbol[8] = TEXT("");
	switch (nBytes)
	{
	case  2:	wsprintf(sSymbol, TEXT("$%02X"), (unsigned)nAddress);  break;
	case  3:	wsprintf(sSymbol, TEXT("$%04X"), (unsigned)nAddress);  break;
		// TODO: FIXME: Can we get called with nBytes == 16 ??
	default:	sSymbol[0] = 0; break; // clear since is static
	}
	return sSymbol;
}

//===========================================================================
char* FormatCharCopy(char* pDst, const char* pSrc, const int nLen)
{
	for (int i = 0; i < nLen; i++)
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
		// But would like to keep compatibility with existing CHARSET40
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
			sprintf(line_.sTargetValue, "%04X", nTarget & 0xFFFF);

			// Always show branch indicators
			bDisasmFormatFlags |= DISASM_FORMAT_BRANCH;

			if (nTarget < nBaseAddress)
				sprintf(line_.sBranch, "%s", g_sConfigBranchIndicatorUp[g_iConfigDisasmBranchType]);
			else
			if (nTarget > nBaseAddress)
				sprintf(line_.sBranch, "%s", g_sConfigBranchIndicatorDown[g_iConfigDisasmBranchType]);
			else
				sprintf(line_.sBranch, "%s", g_sConfigBranchIndicatorEqual[g_iConfigDisasmBranchType]);

			bDisasmFormatFlags |= DISASM_FORMAT_TARGET_POINTER;
			if (g_iConfigDisasmTargets & DISASM_TARGET_ADDR)
				sprintf(line_.sTargetPointer, "%04X", nTarget & 0xFFFF);
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

			const char* pTarget = NULL;
			const char* pSymbol = 0;

			pSymbol = FindSymbolFromAddress(nTarget, &line_.iTargetTable);

			// Data Assembler
			if (pData && (!pData->bSymbolLookup))
				pSymbol = 0;

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
				pTarget = FormatAddress(nTarget, (iOpmode != AM_R) ? nOpbyte : 3);	// GH#587: For Bcc opcodes, pretend it's a 3-byte opcode to print a 16-bit target addr
			}

			//			sprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, pTarget );
			if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
			{
				int nAbsTargetOffset = (line_.nTargetOffset > 0) ? line_.nTargetOffset : -line_.nTargetOffset;
				sprintf(line_.sTargetOffset, "%d", nAbsTargetOffset);
			}
			sprintf(line_.sTarget, "%s", pTarget);


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

				//				if (((iOpmode >= AM_A) && (iOpmode <= AM_NZ)) && (iOpmode != AM_R))
				//					sprintf( sTargetValue_, "%04X", nTargetValue ); // & 0xFFFF

				if (g_iConfigDisasmTargets & DISASM_TARGET_ADDR)
					sprintf(line_.sTargetPointer, "%04X", nTargetPointer & 0xFFFF);

				if (iOpcode != OPCODE_JMP_NA && iOpcode != OPCODE_JMP_IAX)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_TARGET_VALUE;
					if (g_iConfigDisasmTargets & DISASM_TARGET_VAL)
						sprintf(line_.sTargetValue, "%02X", nTargetValue & 0xFF);

					bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
					line_.nImmediate = (BYTE)nTargetValue;

					unsigned _char = FormatCharTxtCtrl(FormatCharTxtHigh(line_.nImmediate, NULL), NULL);
					sprintf(line_.sImmediate, "%c", _char);

					//					if (ConsoleColorIsEscapeMeta( nImmediate_ ))
#if OLD_CONSOLE_COLOR
					if (ConsoleColorIsEscapeMeta(_char))
						sprintf(line_.sImmediate, "%c%c", _char, _char);
					else
						sprintf(line_.sImmediate, "%c", _char);
#endif
				}

				//				if (iOpmode == AM_NA ) // Indirect Absolute
				//					sprintf( sTargetValue_, "%04X", nTargetPointer & 0xFFFF );
				//				else
				// //					sprintf( sTargetValue_, "%02X", nTargetValue & 0xFF );
				//					sprintf( sTargetValue_, "%04X:%02X", nTargetPointer & 0xFFFF, nTargetValue & 0xFF );
			}
		}
		else
			if (iOpmode == AM_M)
			{
				//			sprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, (unsigned)nTarget );
				sprintf(line_.sTarget      , "%02X", (unsigned)nTarget);

				if (nTarget == 0)
					line_.sImmediateSignedDec[0] = 0; // nothing
				else
				if (nTarget < 128)
					sprintf(line_.sImmediateSignedDec, "+%d" , nTarget );
				else
				if (nTarget >= 128)
					sprintf(line_.sImmediateSignedDec, "-%d" , (~nTarget + 1) & 0xFF );

				bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
				line_.nImmediate = (BYTE)nTarget;
				unsigned _char = FormatCharTxtCtrl(FormatCharTxtHigh(line_.nImmediate, NULL), NULL);

				sprintf(line_.sImmediate, "%c", _char);
#if OLD_CONSOLE_COLOR
				if (ConsoleColorIsEscapeMeta(_char))
					sprintf(line_.sImmediate, "%c%c", _char, _char);
				else
					sprintf(line_.sImmediate, "%c", _char);
#endif
			}
	}

	sprintf(line_.sAddress, "%04X", nBaseAddress);

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

	int nSpaces = strlen(line_.sOpCodes);
	while (nSpaces < (int)nMinBytesLen)
	{
		strcat(line_.sOpCodes, " ");
		nSpaces++;
	}

	return bDisasmFormatFlags;
}

//===========================================================================
void FormatOpcodeBytes(WORD nBaseAddress, DisasmLine_t& line_)
{
	int nOpbyte = line_.nOpbyte;

	char* pDst = line_.sOpCodes;
	int nMaxOpBytes = nOpbyte;
	if (nMaxOpBytes > DISASM_DISPLAY_MAX_OPCODES) // 2.8.0.0 fix // TODO: FIX: show max 8 bytes for HEX
		nMaxOpBytes = DISASM_DISPLAY_MAX_OPCODES;

	for (int iByte = 0; iByte < nMaxOpBytes; iByte++)
	{
		BYTE nMem = mem[(nBaseAddress + iByte) & 0xFFFF];
		sprintf(pDst, "%02X", nMem); // sBytes+strlen(sBytes)
		pDst += 2;

		// TODO: If Disassembly_IsDataAddress() don't show spaces...
		if (g_bConfigDisasmOpcodeSpaces)
		{
			strcat(pDst, " ");
			pDst++; // 2.5.3.3 fix
		}
	}
}

struct FAC_t
{
	uint8_t negative;
	 int8_t exponent;
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
	char* pDst = line_.sTarget;
	const	char* pSrc = 0;
	DWORD nStartAddress = line_.pDisasmData->nStartAddress;
	DWORD nEndAddress   = line_.pDisasmData->nEndAddress;
	//		int   nDataLen      = nEndAddress - nStartAddress + 1 ;
	int   nDisplayLen = nEndAddress - nBaseAddress + 1; // *inclusive* KEEP IN SYNC: _CmdDefineByteRange() CmdDisasmDataList() _6502_GetOpmodeOpbyte() FormatNopcodeBytes()
	int   len = nDisplayLen;

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
				sprintf(pDst, "%02X", nTarget8); // sBytes+strlen(sBytes)
				pDst += 2;
				iByte++;
				if (line_.iNoptype == NOP_BYTE_1)
					if (iByte < line_.nOpbyte)
					{
						*pDst++ = ',';
					}
				break;

			case NOP_FAC:
			{
				FAC_t fac;
				FAC_Unpack( nBaseAddress, fac );
				const char aSign[2] = { '+', '-' };
				if (fac.isZero)
					sprintf( pDst, "0" );
				else
				{
					double f = fac.mantissa * pow( 2.0, fac.exponent - 32 );
					//sprintf( "s%1X m%04X e%02X", fac.negative, fac.mantissa, fac.exponent );
					sprintf( pDst, "%c%f", aSign[ fac.negative ], f );
				}
				iByte += 5;
				break;
			}

			case NOP_WORD_1:
			case NOP_WORD_2:
			case NOP_WORD_4:
				sprintf(pDst, "%04X", nTarget16); // sBytes+strlen(sBytes)
				pDst += 4;
				iByte += 2;
				if (iByte < line_.nOpbyte)
				{
					*pDst++ = ',';
				}
				break;

			case NOP_ADDRESS:
				// Nothing to do, already handled :-)
				iByte += 2;
				break;

			case NOP_STRING_APPLESOFT:
				iByte = line_.nOpbyte;
				strncpy(pDst, (const char*)(mem + nBaseAddress), iByte);
				pDst += iByte;
				*pDst = 0;
			case NOP_STRING_APPLE:
				iByte = line_.nOpbyte; // handle all bytes of text
				pSrc = (const char*)mem + nStartAddress;

				if (len > (DISASM_DISPLAY_MAX_IMMEDIATE_LEN - 2)) // does "text" fit?
				{
					if (len > DISASM_DISPLAY_MAX_IMMEDIATE_LEN) // no; need extra characters for ellipsis?
						len = (DISASM_DISPLAY_MAX_IMMEDIATE_LEN - 3); // ellipsis = true

					// DISPLAY: text_longer_18...
					FormatCharCopy(pDst, pSrc, len); // BUG: #251 v2.8.0.7: ASC #:# with null byte doesn't mark up properly

					if (nDisplayLen > len) // ellipsis
					{
						*pDst++ = '.';
						*pDst++ = '.';
						*pDst++ = '.';
					}
				}
				else { // DISPLAY: "max_18_char"
					*pDst++ = '"';
					pDst = FormatCharCopy(pDst, pSrc, len); // BUG: #251 v2.8.0.7: ASC #:# with null byte doesn't mark up properly
					*pDst++ = '"';
				}

				*pDst = 0;
				break;

			default:
	#if _DEBUG // Unhandled data disassembly!
				int* FATAL = 0;
				*FATAL = 0xDEADC0DE;
	#endif
				iByte++;
				break;
		}
	}
}

//===========================================================================
void FormatDisassemblyLine(const DisasmLine_t& line, char* sDisassembly, const int nBufferSize)
{
	//> Address Seperator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//
	// Data Disassembler
	//                              Label Directive       [Immediate]
	const char* pMnemonic = g_aOpcodes[line.iOpcode].sMnemonic;

	sprintf(sDisassembly, "%s:%s %s "
		, line.sAddress
		, line.sOpCodes
		, pMnemonic
	);

	/*
		if (line.bTargetIndexed || line.bTargetIndirect)
		{
			strcat( sDisassembly, "(" );
		}

		if (line.bTargetImmediate)
			strcat( sDisassembly, "#$" );

		if (line.bTargetValue)
			strcat( sDisassembly, line.sTarget );

		if (line.bTargetIndirect)
		{
			if (line.bTargetX)
				strcat( sDisassembly, ", X" );
			if (line.bTargetY)
				strcat( sDisassembly, ", Y" );
		}

		if (line.bTargetIndexed || line.bTargetIndirect)
		{
			strcat( sDisassembly, ")" );
		}

		if (line.bTargetIndirect)
		{
			if (line.bTargetY)
				strcat( sDisassembly, ", Y" );
		}
	*/
	char sTarget[32];

	if (line.bTargetValue || line.bTargetRelative || line.bTargetImmediate)
	{
		if (line.bTargetRelative)
			strcpy(sTarget, line.sTargetValue);
		else
			if (line.bTargetImmediate)
			{
				strcat(sDisassembly, "#");
				strncpy(sTarget, line.sTarget, sizeof(sTarget));
				sTarget[sizeof(sTarget) - 1] = 0;
			}
			else
				sprintf(sTarget, g_aOpmodes[line.iOpmode].m_sFormat, line.nTarget);

		strcat(sDisassembly, "$");
		strcat(sDisassembly, sTarget);
	}
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
		TCHAR sText[CONSOLE_WIDTH * 2];
		sprintf(sText, TEXT("DisasmCalcTopFromCurAddress()\n"
			"\tTop: %04X\n"
			"\tLen: %04X\n"
			"\tMissed: %04X"),
			g_nDisasmCurAddress - nLen, nLen, g_nDisasmCurAddress);
		GetFrame().FrameMessageBox(sText, "ERROR", MB_OK);
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
