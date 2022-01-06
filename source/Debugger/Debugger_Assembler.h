#ifndef DEBUGGER_ASSEMBLER_H
#define DEBUGGER_ASSEMBLER_H

// Directives

	// Assemblers
	//     A = Acme
	//     B = Big Mac            S= S-C Macro Assembler
	//     D = DOS Tool Kit       T = TED II
	//     L = Lisa               W = Weller's Assembler
	//     M = Merlin
	//     u = MicroSparc
	//     O = ORCA/M
	enum Assemblers_e
	{
		  ASM_ACME
		, ASM_BIG_MAC
		, ASM_DOS_TOOL_KIT
		, ASM_LISA
		, ASM_MERLIN
		, ASM_MICROSPARC
		, ASM_ORCA
		, ASM_SC
		, ASM_TED
		, ASM_WELLERS
		, ASM_CUSTOM
	,NUM_ASSEMBLERS
	};

	enum AsmAcmeDirective_e
	{
		 ASM_A_DEFINE_BYTE
	,NUM_ASM_A_DIRECTIVES
	};

	enum AsmBigMacDirective_e
	{
		 ASM_B_DEFINE_BYTE
	,NUM_ASM_B_DIRECTIVES
	};

	enum AsmDosToolKitDirective_e
	{
		 ASM_D_DEFINE_BYTE
	,NUM_ASM_D_DIRECTIVES
	};

	enum AsmLisaDirective_e
	{
		 ASM_L_DEFINE_BYTE
	,NUM_ASM_L_DIRECTIVES
	};

	enum AsmMerlinDirective_e
	{
		  ASM_M_ASCII
		, ASM_M_DEFINE_WORD
		, ASM_M_DEFINE_BYTE
		, ASM_M_DEFINE_STORAGE
		, ASM_M_HEX
		, ASM_M_ORIGIN
	, NUM_ASM_M_DIRECTIVES
		, ASM_M_DEFINE_BYTE_ALIAS
		, ASM_M_DEFINE_WORD_ALIAS
	};

	enum AsmMicroSparcDirective_e
	{
		 ASM_u_DEFINE_BYTE
	,NUM_ASM_u_DIRECTIVES
	};

	enum AsmOrcamDirective_e
	{
		 ASM_O_DEFINE_BYTE
	,NUM_ASM_O_DIRECTIVES
	};

	enum AsmSCMacroDirective_e
	{
		 ASM_S_ORIGIN
		,ASM_S_TARGET_ADDRESS
		,ASM_S_END_PROGRAM
		,ASM_S_EQUATE
		,ASM_S_DATA
		,ASM_S_ASCII_STRING
		,ASM_S_HEX_STRING
	,NUM_ASM_S_DIRECTIVES
	};

	enum AsmTedDirective_e
	{
		 ASM_T_DEFINE_BYTE
	,NUM_ASM_T_DIRECTIVES
	};

	enum AsmWellersDirective_e
	{
		 ASM_W_DEFINE_BYTE
	,NUM_ASM_W_DIRECTIVES
	};

	// NOTE: Keep in sync!  AsmCustomDirective_e  g_aAssemblerDirectives
	enum AsmCustomDirective_e
	{
		 ASM_DEFINE_BYTE	
		,ASM_DEFINE_WORD
//		,ASM_DEFINE_ADDRESS_8
		,ASM_DEFINE_ADDRESS_16
		// String/Text/Ascii
		,ASM_DEFINE_ASCII_TEXT
		,ASM_DEFINE_APPLE_TEXT
		,ASM_DEFINE_TEXT_HI_LO // i.e. Applesoft Basic Tokens
		// FAC
		,ASM_DEFINE_FLOAT      // Applesoft float
		,ASM_DEFINE_FLOAT_X    // Applesoft float unpacked/expanded
	,NUM_ASM_Z_DIRECTIVES
	};

	// NOTE: Keep in sync AsmDirectives_e g_aAssemblerDirectives !
	enum AsmDirectives_e
	{
		FIRST_A_DIRECTIVE = 1                                       , // Acme
		FIRST_B_DIRECTIVE = FIRST_A_DIRECTIVE + NUM_ASM_A_DIRECTIVES, // Big Mac
		FIRST_D_DIRECTIVE = FIRST_B_DIRECTIVE + NUM_ASM_B_DIRECTIVES, // DOS Tool Kit
		FIRST_L_DIRECTIVE = FIRST_D_DIRECTIVE + NUM_ASM_D_DIRECTIVES, // Lisa
		FIRST_M_DIRECTIVE = FIRST_L_DIRECTIVE + NUM_ASM_L_DIRECTIVES, // Merlin
		FIRST_u_DIRECTIVE = FIRST_M_DIRECTIVE + NUM_ASM_M_DIRECTIVES, // MicroSparc
		FIRST_O_DIRECTIVE = FIRST_u_DIRECTIVE + NUM_ASM_u_DIRECTIVES, // Orca
		FIRST_S_DIRECTIVE = FIRST_O_DIRECTIVE + NUM_ASM_O_DIRECTIVES, // SC
		FIRST_T_DIRECTIVE = FIRST_S_DIRECTIVE + NUM_ASM_S_DIRECTIVES, // Ted
		FIRST_W_DIRECTIVE = FIRST_T_DIRECTIVE + NUM_ASM_T_DIRECTIVES, // Weller
		FIRST_Z_DIRECTIVE = FIRST_W_DIRECTIVE + NUM_ASM_W_DIRECTIVES, // Custom
	NUM_ASM_DIRECTIVES    = FIRST_Z_DIRECTIVE + NUM_ASM_Z_DIRECTIVES  

//		NUM_ASM_DIRECTIVES =  1 +  // Opcode ... rest are psuedo opcodes
//			NUM_ASM_A_DIRECTIVES + // Acme
//			NUM_ASM_B_DIRECTIVES + // Big Mac
//			NUM_ASM_D_DIRECTIVES + // DOS Tool Kit
//			NUM_ASM_L_DIRECTIVES + // Lisa
//			NUM_ASM_M_DIRECTIVES + // Merlin
//			NUM_ASM_u_DIRECTIVES + // MicroSparc
//			NUM_ASM_O_DIRECTIVES + // Orca
//			NUM_ASM_S_DIRECTIVES + // SC
//			NUM_ASM_T_DIRECTIVES + // Ted
//			NUM_ASM_W_DIRECTIVES   // Weller
	};

extern	int g_iAssemblerSyntax;
extern	int g_aAssemblerFirstDirective[ NUM_ASSEMBLERS ];

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
		const char  *m_pMnemonic;
		Hash_t m_nHash;
	};

	extern int    g_bAssemblerOpcodesHashed; // = false;
	extern Hash_t g_aOpcodesHash[ NUM_OPCODES ]; // for faster mnemonic lookup, for the assembler
	extern bool   g_bAssemblerInput; // = false;
	extern int    g_nAssemblerAddress; // = 0;

	extern const Opcodes_t *g_aOpcodes; // = NULL; // & g_aOpcodes65C02[ 0 ];

	extern const Opcodes_t g_aOpcodes65C02[ NUM_OPCODES ];
	extern const Opcodes_t g_aOpcodes6502 [ NUM_OPCODES ];

	extern AssemblerDirective_t g_aAssemblerDirectives[ NUM_ASM_DIRECTIVES ];

// Prototypes _______________________________________________________________

	int  _6502_GetOpmodeOpbyte( const int iAddress, int & iOpmode_, int & nOpbytes_, const DisasmData_t** pData = NULL );
	void _6502_GetOpcodeOpmodeOpbyte( int & iOpcode_, int & iOpmode_, int & nOpbytes_ );
	bool _6502_GetStackReturnAddress( WORD & nAddress_ );
	bool _6502_GetTargets( WORD nAddress, int *pTargetPartial_, int *pTargetPartial2_, int *pTargetPointer_, int * pBytes_,
						   bool bIgnoreBranch = true, bool bIncludeNextOpcodeAddress = true );
	bool _6502_GetTargetAddress( const WORD & nAddress, WORD & nTarget_ );
	bool _6502_IsOpcodeBranch( int nOpcode );
	bool _6502_IsOpcodeValid( int nOpcode );

	Hash_t  AssemblerHashMnemonic ( const TCHAR * pMnemonic );
//	bool AssemblerGetAddressingMode ( int iArg, int nArgs, WORD nAddress, std::vector<int> & vOpcodes );
	void _CmdAssembleHashDump ();
	
	int AssemblerDelayedTargetsSize();
	void AssemblerStartup ();
	bool Assemble( int iArg, int nArgs, WORD nAddress );

	void AssemblerOn  ();
	void AssemblerOff ();

#endif
