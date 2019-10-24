
// Variables
	extern 	SymbolTable_t g_aSymbols[ NUM_SYMBOL_TABLES ];

// Prototypes

	Update_t _CmdSymbolsClear ( SymbolTable_Index_e eSymbolTable );
	Update_t _CmdSymbolsCommon ( int nArgs, SymbolTable_Index_e eSymbolTable );
	Update_t _CmdSymbolsListTables (int nArgs, int bSymbolTables );
	Update_t _CmdSymbolsUpdate ( int nArgs, int bSymbolTables );

	bool _CmdSymbolList_Address2Symbol ( int nAddress   , int bSymbolTables );
	bool _CmdSymbolList_Symbol2Address ( LPCTSTR pSymbol, int bSymbolTables );

	// SymbolOffset
	int ParseSymbolTable ( const std::string & pFileName, SymbolTable_Index_e eWhichTableToLoad, int nSymbolOffset = 0 );

