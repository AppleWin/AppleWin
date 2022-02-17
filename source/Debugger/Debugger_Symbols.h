
// Variables
	extern 	SymbolTable_t g_aSymbols[ NUM_SYMBOL_TABLES ];
	extern bool g_bSymbolsDisplayMissingFile;

// Prototypes

	Update_t _CmdSymbolsClear ( SymbolTable_Index_e eSymbolTable );
	Update_t _CmdSymbolsListTables (int nArgs, int bSymbolTables );
	Update_t _CmdSymbolsUpdate ( int nArgs, int bSymbolTables );

	bool _CmdSymbolList_Address2Symbol ( int nAddress   , int bSymbolTables );
	bool _CmdSymbolList_Symbol2Address ( LPCTSTR pSymbol, int bSymbolTables );

	// SymbolOffset
	int ParseSymbolTable ( const std::string & pFileName, SymbolTable_Index_e eWhichTableToLoad, int nSymbolOffset = 0 );

	// Symbol Table / Memory
	bool FindAddressFromSymbol(const char* pSymbol, WORD* pAddress_ = NULL, int* iTable_ = NULL);
	WORD GetAddressFromSymbol(const char* symbol); // HACK: returns 0 if symbol not found
	void SymbolUpdate(SymbolTable_Index_e eSymbolTable, const char* pSymbolName, WORD nAddrss, bool bRemoveSymbol, bool bUpdateSymbol);
	std::string const* FindSymbolFromAddress(WORD nAdress, int* iTable_ = NULL);
	std::string const& GetSymbol(WORD nAddress, int nBytes, std::string& strAddressBuf);
