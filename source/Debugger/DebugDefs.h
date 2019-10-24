#pragma once

	const          int _6502_BRANCH_POS      = +127;
	const          int _6502_BRANCH_NEG      = -128;
	const unsigned int _6502_ZEROPAGE_END    = 0x00FF;
	const unsigned int _6502_STACK_BEGIN     = 0x0100;
	const unsigned int _6502_STACK_END       = 0x01FF;
	const unsigned int _6502_IO_BEGIN        = 0xC000;
	const unsigned int _6502_IO_END          = 0xC0FF;
	const unsigned int _6502_BRK_VECTOR      = 0xFFFE;
	const unsigned int _6502_MEM_BEGIN = 0x0000;
	const unsigned int _6502_MEM_END   = 0xFFFF;
	const unsigned int _6502_MEM_LEN   = _6502_MEM_END + 1;
