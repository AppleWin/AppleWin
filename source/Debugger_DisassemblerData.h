#ifndef DEBUGGER_DISASSEMBLERDATA_H
#define DEBUGGER_DISASSEMBLERDATA_H

	enum NopcodeType_e
	{
		 NOP_BYTE_1 // 1 bytes/line
		,NOP_BYTE_2 // 2 bytes/line
		,NOP_BYTE_4 // 4 bytes/line
		,NOP_BYTE_8 // 8 bytes/line
		,NOP_WORD_1 // 1 words/line
		,NOP_WORD_2 // 2 words/line
		,NOP_WORD_4 // 4 words/line
		,NOP_ADDRESS// 1 word/line
		,NOP_HEX
		,NOP_CHAR
		,NOP_STRING_ASCII
		,NOP_STRING_APPLE
		,NOP_STRING_APPLESOFT
		,NOP_FAC
		,NUM_NOPCODE_TYPES
	};

	// Disassembler Data
	// type symbol[start:end]
	struct DisasmData_t
	{
		char sSymbol[ MAX_SYMBOLS_LEN+1 ];
		WORD iDirective   ; // Assembler directive -> nopcode
		WORD nStartAddress; // link to block [start,end)
		WORD nEndAddress  ; 
		WORD nArraySize   ; // Total bytes
//		WORD nBytePerRow  ; // 1, 8
		char eElementType; // 

		// with symbol lookup
		char bSymbolLookup ;
		WORD nTargetAddress;
	};

	Update_t _CmdDisasmDataDefByteX    (int nArgs);
	Update_t _CmdDisasmDataDefWordX    (int nArgs);

// Data Disassembler ______________________________________________________________________________

	int Disassembly_FindOpcode( WORD nAddress );
	DisasmData_t* Disassembly_IsDataAddress( WORD nAddress );

	void Disassembly_AddData( DisasmData_t tData);
	void Disassembly_GetData ( WORD nBaseAddress, const DisasmData_t *pData_, DisasmLine_t & line_ );
	void Disassembly_DelData( DisasmData_t tData);
	DisasmData_t* Disassembly_Enumerate( DisasmData_t *pCurrent = NULL );

	extern vector<DisasmData_t> g_aDisassemblerData;

#endif
