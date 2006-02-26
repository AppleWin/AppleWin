#ifndef DEBUGGER_ASSEMBLER_H
#define DEBUGGER_ASSEMBLER_H

// Addressing _____________________________________________________________________________________

	extern AddressingMode_t g_aOpmodes[ NUM_ADDRESSING_MODES ];

// Assembler ______________________________________________________________________________________

	// Hashing for Assembler
	typedef unsigned int Hash_t;

	struct HashOpcode_t
	{
		int    m_iOpcode;
		Hash_t m_nValue;

		// functor
		bool operator () (const HashOpcode_t & rLHS, const HashOpcode_t & rRHS) const
		{
			bool bLessThan = (rLHS.m_nValue < rRHS.m_nValue);
			return bLessThan;
		}
	};

	struct AssemblerDirective_t
	{
		char   m_sMnemonic[ MAX_MNEMONIC_LEN + 1 ];	
		Hash_t m_nHash;
	};

	extern int    g_bAssemblerOpcodesHashed; // = false;
	extern Hash_t g_aOpcodesHash[ NUM_OPCODES ]; // for faster mnemonic lookup, for the assembler
	extern bool   g_bAssemblerInput; // = false;
	extern int    g_nAssemblerAddress; // = 0;

	extern const Opcodes_t *g_aOpcodes; // = NULL; // & g_aOpcodes65C02[ 0 ];

	extern const Opcodes_t g_aOpcodes65C02[ NUM_OPCODES ];
	extern const Opcodes_t g_aOpcodes6502 [ NUM_OPCODES ];

// Prototypes _______________________________________________________________

	int  _6502GetOpmodeOpbytes       ( const int iAddress, int & iOpmode_, int & nOpbytes_ );
//	void _6502GetOpcodeOpmode        ( int & iOpcode_, int & iOpmode_, int & nOpbytes_ );
	void _6502GetOpcodeOpmodeOpbytes ( int & iOpcode_, int & iOpmode_, int & nOpbytes_ );

	int  AssemblerHashMnemonic ( const TCHAR * pMnemonic );
	void AssemblerHashOpcodes ();
	void AssemblerHashMerlinDirectives ();
//	bool AssemblerGetAddressingMode ( int iArg, int nArgs, WORD nAddress, vector<int> & vOpcodes );
	void _CmdAssembleHashDump ();
	
	int AssemblerDelayedTargetsSize();
	void AssemblerStartup ();
	bool Assemble( int iArg, int nArgs, WORD nAddress );

	void AssemblerOn  ();
	void AssemblerOff ();

#endif
