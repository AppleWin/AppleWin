#pragma once

// Apple II defines & memory locations
enum
{
	// Note: All are in bytes!
	TEXT_PAGE1_BEGIN         = 0x0400,
	TEXT_PAGE1_SIZE          = 0x0400,

	APPLE_SLOT_SIZE          = 0x0100, // 1 page  = $Cx00 .. $CxFF (slot 1 .. 7)
	APPLE_IO_BEGIN           = 0xC000,
	APPLE_IO_END             = 0xC0FF,
	APPLE_SLOT_BEGIN         = 0xC100, // each slot has 1 page reserved for it
	APPLE_SLOT_END           = 0xC7FF,

	FIRMWARE_EXPANSION_SIZE  = 0x0800, // 8 pages = $C800 .. $CFFF
	FIRMWARE_EXPANSION_BEGIN = 0xC800, // [C800,CFFF)
	FIRMWARE_EXPANSION_END   = 0xCFFF
};

// 6502 defines & memory locations
enum
{
	_6502_BRANCH_POS		= +127,
	_6502_BRANCH_NEG		= -128,
	_6502_PAGE_SIZE         = 0x0100,

	_6502_ZEROPAGE_END		= 0x00FF,
	_6502_STACK_BEGIN		= 0x0100,
	_6502_STACK_END			= 0x01FF,
	_6502_NMI_VECTOR		= 0xFFFA,
	_6502_RESET_VECTOR		= 0xFFFC,
	_6502_INTERRUPT_VECTOR	= 0xFFFE,	/* IRQ & BRK */
	_6502_MEM_BEGIN			= 0x0000,
	_6502_MEM_END			= 0xFFFF,
	_6502_MEM_LEN			= _6502_MEM_END + 1,

	_6502_ZERO_PAGE         = _6502_MEM_BEGIN >> 8,
	_6502_STACK_PAGE        = _6502_STACK_BEGIN >> 8
};
