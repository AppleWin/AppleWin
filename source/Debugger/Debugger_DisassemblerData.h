#ifndef DEBUGGER_DISASSEMBLERDATA_H
#define DEBUGGER_DISASSEMBLERDATA_H

	Update_t _CmdDisasmDataDefByteX    (int nArgs);
	Update_t _CmdDisasmDataDefWordX    (int nArgs);

// Data Disassembler ______________________________________________________________________________

	int Disassembly_FindOpcode( WORD nAddress );
	DisasmData_t* Disassembly_IsDataAddress( WORD nAddress );

	void Disassembly_AddData( DisasmData_t tData);
	void Disassembly_GetData ( WORD nBaseAddress, const DisasmData_t *pData_, DisasmLine_t & line_ );
	void Disassembly_DelData( DisasmData_t tData);
	DisasmData_t* Disassembly_Enumerate( DisasmData_t *pCurrent = NULL );

	extern std::vector<DisasmData_t> g_aDisassemblerData;

#endif
