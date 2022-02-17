#pragma once

int GetDisassemblyLine(const WORD nOffset, DisasmLine_t& line_);
//		, int iOpcode, int iOpmode, int nOpbytes
//		char *sAddress_, char *sOpCodes_,
//		char *sTarget_, char *sTargetOffset_, int & nTargetOffset_, char *sTargetValue_,
//		char * sImmediate_, char & nImmediate_, char *sBranch_ );
void FormatDisassemblyLine(const DisasmLine_t& line, char* sDisassembly_, const int nBufferSize);
void FormatOpcodeBytes(WORD nBaseAddress, DisasmLine_t& line_);
void FormatNopcodeBytes(WORD nBaseAddress, DisasmLine_t& line_);

std::string FormatAddress(WORD nAddress, int nBytes);
char* FormatCharCopy(char* pDst, const char* pSrc, const int nLen);

char  FormatCharTxtAsci(const BYTE b, bool* pWasAsci_ = NULL);
char  FormatCharTxtCtrl(const BYTE b, bool* pWasCtrl_ = NULL);
char  FormatCharTxtHigh(const BYTE b, bool* pWasHi_ = NULL);
char  FormatChar4Font(const BYTE b, bool* pWasHi_, bool* pWasLo_);

void GetTargets_IgnoreDirectJSRJMP(const BYTE iOpcode, int& nTargetPointer);

void DisasmCalcTopFromCurAddress(bool bUpdateTop = true);
void DisasmCalcCurFromTopAddress();
void DisasmCalcBotFromTopAddress();
void DisasmCalcTopBotAddress();
WORD DisasmCalcAddressFromLines(WORD iAddress, int nLines);
