#pragma once

int GetDisassemblyLine(const WORD nOffset, DisasmLine_t& line_);
std::string FormatDisassemblyLine(const DisasmLine_t& line);
void FormatOpcodeBytes(WORD nBaseAddress, DisasmLine_t& line_);
void FormatNopcodeBytes(WORD nBaseAddress, DisasmLine_t& line_);

std::string FormatAddress(WORD nAddress, int nBytes);
char* FormatCharCopy(char* pDst, const char* pEnd, const char* pSrc, const int nLen);

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
