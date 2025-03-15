#pragma once

enum
{
	// Note: All are in bytes!
	TEXT_PAGE1_BEGIN         = 0x0400,
	TEXT_PAGE1_SIZE          = 0x0400,

	APPLE_SLOT_SIZE          = 0x0100, // 1 page  = $Cx00 .. $CxFF (slot 1 .. 7)
	APPLE_IO_BEGIN           = 0xC000,
	APPLE_SLOT_BEGIN         = 0xC100, // each slot has 1 page reserved for it
	APPLE_SLOT_END           = 0xC7FF, //

	FIRMWARE_EXPANSION_SIZE  = 0x0800, // 8 pages = $C800 .. $CFFF
	FIRMWARE_EXPANSION_BEGIN = 0xC800, // [C800,CFFF)
	FIRMWARE_EXPANSION_END   = 0xCFFF, //

	PAGE_SIZE                = 0x100,
	MEMORY_LENGTH            = 0x10000
};
