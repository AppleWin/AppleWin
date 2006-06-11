/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Memory emulation
 *
 * Author: Various
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"

#define  MF_80STORE    0x00000001
#define  MF_ALTZP      0x00000002
#define  MF_AUXREAD    0x00000004
#define  MF_AUXWRITE   0x00000008
#define  MF_BANK2      0x00000010
#define  MF_HIGHRAM    0x00000020
#define  MF_HIRES      0x00000040
#define  MF_PAGE2      0x00000080
#define  MF_SLOTC3ROM  0x00000100
#define  MF_SLOTCXROM  0x00000200
#define  MF_WRITERAM   0x00000400
#define  MF_IMAGEMASK  0x000003F7

#define  SW_80STORE    (memmode & MF_80STORE)
#define  SW_ALTZP      (memmode & MF_ALTZP)
#define  SW_AUXREAD    (memmode & MF_AUXREAD)
#define  SW_AUXWRITE   (memmode & MF_AUXWRITE)
#define  SW_BANK2      (memmode & MF_BANK2)
#define  SW_HIGHRAM    (memmode & MF_HIGHRAM)
#define  SW_HIRES      (memmode & MF_HIRES)
#define  SW_PAGE2      (memmode & MF_PAGE2)
#define  SW_SLOTC3ROM  (memmode & MF_SLOTC3ROM)
#define  SW_SLOTCXROM  (memmode & MF_SLOTCXROM)
#define  SW_WRITERAM   (memmode & MF_WRITERAM)

BYTE __stdcall NullIo (WORD programcounter, BYTE address, BYTE write, BYTE value, ULONG nCycles);

iofunction ioread[0x100]  = {KeybReadData,       // $C000
                             KeybReadData,       // $C001
                             KeybReadData,       // $C002
                             KeybReadData,       // $C003
                             KeybReadData,       // $C004
                             KeybReadData,       // $C005
                             KeybReadData,       // $C006
                             KeybReadData,       // $C007
                             KeybReadData,       // $C008
                             KeybReadData,       // $C009
                             KeybReadData,       // $C00A
                             KeybReadData,       // $C00B
                             KeybReadData,       // $C00C
                             KeybReadData,       // $C00D
                             KeybReadData,       // $C00E
                             KeybReadData,       // $C00F
                             KeybReadFlag,       // $C010
                             MemCheckPaging,     // $C011
                             MemCheckPaging,     // $C012
                             MemCheckPaging,     // $C013
                             MemCheckPaging,     // $C014
                             MemCheckPaging,     // $C015
                             MemCheckPaging,     // $C016
                             MemCheckPaging,     // $C017
                             MemCheckPaging,     // $C018
                             VideoCheckVbl,      // $C019
                             VideoCheckMode,     // $C01A
                             VideoCheckMode,     // $C01B
                             MemCheckPaging,     // $C01C
                             MemCheckPaging,     // $C01D
                             VideoCheckMode,     // $C01E
                             VideoCheckMode,     // $C01F
                             NullIo,             // $C020
                             NullIo,             // $C021
                             NullIo,             // $C022
                             NullIo,             // $C023
                             NullIo,             // $C024
                             NullIo,             // $C025
                             NullIo,             // $C026
                             NullIo,             // $C027
                             NullIo,             // $C028
                             NullIo,             // $C029
                             NullIo,             // $C02A
                             NullIo,             // $C02B
                             NullIo,             // $C02C
                             NullIo,             // $C02D
                             NullIo,             // $C02E
                             NullIo,             // $C02F
                             SpkrToggle,         // $C030
                             SpkrToggle,         // $C031
                             SpkrToggle,         // $C032
                             SpkrToggle,         // $C033
                             SpkrToggle,         // $C034
                             SpkrToggle,         // $C035
                             SpkrToggle,         // $C036
                             SpkrToggle,         // $C037
                             SpkrToggle,         // $C038
                             SpkrToggle,         // $C039
                             SpkrToggle,         // $C03A
                             SpkrToggle,         // $C03B
                             SpkrToggle,         // $C03C
                             SpkrToggle,         // $C03D
                             SpkrToggle,         // $C03E
                             SpkrToggle,         // $C03F
                             NullIo,             // $C040
                             NullIo,             // $C041
                             NullIo,             // $C042
                             NullIo,             // $C043
                             NullIo,             // $C044
                             NullIo,             // $C045
                             NullIo,             // $C046
                             NullIo,             // $C047
                             NullIo,             // $C048
                             NullIo,             // $C049
                             NullIo,             // $C04A
                             NullIo,             // $C04B
                             NullIo,             // $C04C
                             NullIo,             // $C04D
                             NullIo,             // $C04E
                             NullIo,             // $C04F
                             VideoSetMode,       // $C050
                             VideoSetMode,       // $C051
                             VideoSetMode,       // $C052
                             VideoSetMode,       // $C053
                             MemSetPaging,       // $C054
                             MemSetPaging,       // $C055
                             MemSetPaging,       // $C056
                             MemSetPaging,       // $C057
                             NullIo,             // $C058
                             NullIo,             // $C059
                             NullIo,             // $C05A
                             NullIo,             // $C05B
                             NullIo,             // $C05C
                             NullIo,             // $C05D
                             VideoSetMode,       // $C05E
                             VideoSetMode,       // $C05F
                             NullIo,             // $C060
                             JoyReadButton,      // $C061
                             JoyReadButton,      // $C062
                             JoyReadButton,      // $C063
                             JoyReadPosition,    // $C064
                             JoyReadPosition,    // $C065
                             JoyReadPosition,    // $C066
                             JoyReadPosition,    // $C067
                             NullIo,             // $C068
                             NullIo,             // $C069
                             NullIo,             // $C06A
                             NullIo,             // $C06B
                             NullIo,             // $C06C
                             NullIo,             // $C06D
                             NullIo,             // $C06E
                             NullIo,             // $C06F
                             JoyResetPosition,   // $C070
                             NullIo,             // $C071
                             NullIo,             // $C072
                             NullIo,             // $C073
                             NullIo,             // $C074
                             NullIo,             // $C075
                             NullIo,             // $C076
                             NullIo,             // $C077
                             NullIo,             // $C078
                             NullIo,             // $C079
                             NullIo,             // $C07A
                             NullIo,             // $C07B
                             NullIo,             // $C07C
                             NullIo,             // $C07D
                             NullIo,             // $C07E
                             VideoCheckMode,     // $C07F
                             MemSetPaging,       // $C080
                             MemSetPaging,       // $C081
                             MemSetPaging,       // $C082
                             MemSetPaging,       // $C083
                             MemSetPaging,       // $C084
                             MemSetPaging,       // $C085
                             MemSetPaging,       // $C086
                             MemSetPaging,       // $C087
                             MemSetPaging,       // $C088
                             MemSetPaging,       // $C089
                             MemSetPaging,       // $C08A
                             MemSetPaging,       // $C08B
                             MemSetPaging,       // $C08C
                             MemSetPaging,       // $C08D
                             MemSetPaging,       // $C08E
                             MemSetPaging,       // $C08F
                             NullIo,             // $C090
                             NullIo,             // $C091
                             NullIo,             // $C092
                             NullIo,             // $C093
                             NullIo,             // $C094
                             NullIo,             // $C095
                             NullIo,             // $C096
                             NullIo,             // $C097
                             NullIo,             // $C098
                             NullIo,             // $C099
                             NullIo,             // $C09A
                             NullIo,             // $C09B
                             NullIo,             // $C09C
                             NullIo,             // $C09D
                             NullIo,             // $C09E
                             NullIo,             // $C09F
                             NullIo,             // $C0A0
                             CommDipSw,          // $C0A1
                             CommDipSw,          // $C0A2
                             NullIo,             // $C0A3
                             NullIo,             // $C0A4
                             NullIo,             // $C0A5
                             NullIo,             // $C0A6
                             NullIo,             // $C0A7
                             CommReceive,        // $C0A8
                             CommStatus,         // $C0A9
                             CommCommand,        // $C0AA
                             CommControl,        // $C0AB
                             NullIo,             // $C0AC
                             NullIo,             // $C0AD
                             NullIo,             // $C0AE
                             NullIo,             // $C0AF
                             TfeIo,              // $C0B0
                             TfeIo,              // $C0B1
                             TfeIo,              // $C0B2
                             TfeIo,              // $C0B3
                             TfeIo,              // $C0B4
                             TfeIo,              // $C0B5
                             TfeIo,              // $C0B6
                             TfeIo,              // $C0B7
                             TfeIo,              // $C0B8
                             TfeIo,              // $C0B9
                             TfeIo,              // $C0BA
                             TfeIo,              // $C0BB
                             TfeIo,              // $C0BC
                             TfeIo,              // $C0BD
                             TfeIo,              // $C0BE
                             TfeIo,              // $C0BF
                             PhasorIO,           // $C0C0
                             PhasorIO,           // $C0C1
                             PhasorIO,           // $C0C2
                             PhasorIO,           // $C0C3
                             PhasorIO,           // $C0C4
                             PhasorIO,           // $C0C5
                             PhasorIO,           // $C0C6
                             PhasorIO,           // $C0C7
                             PhasorIO,           // $C0C8
                             PhasorIO,           // $C0C9
                             PhasorIO,           // $C0CA
                             PhasorIO,           // $C0CB
                             PhasorIO,           // $C0CC
                             PhasorIO,           // $C0CD
                             PhasorIO,           // $C0CE
                             PhasorIO,           // $C0CF
                             PhasorIO,           // $C0D0
                             PhasorIO,           // $C0D1
                             PhasorIO,           // $C0D2
                             PhasorIO,           // $C0D3
                             PhasorIO,           // $C0D4
                             PhasorIO,           // $C0D5
                             PhasorIO,           // $C0D6
                             PhasorIO,           // $C0D7
                             PhasorIO,           // $C0D8
                             PhasorIO,           // $C0D9
                             PhasorIO,           // $C0DA
                             PhasorIO,           // $C0DB
                             PhasorIO,           // $C0DC
                             PhasorIO,           // $C0DD
                             PhasorIO,           // $C0DE
                             PhasorIO,           // $C0DF
                             DiskControlStepper, // $C0E0
                             DiskControlStepper, // $C0E1
                             DiskControlStepper, // $C0E2
                             DiskControlStepper, // $C0E3
                             DiskControlStepper, // $C0E4
                             DiskControlStepper, // $C0E5
                             DiskControlStepper, // $C0E6
                             DiskControlStepper, // $C0E7
                             DiskControlMotor,   // $C0E8
                             DiskControlMotor,   // $C0E9
                             DiskEnable,         // $C0EA
                             DiskEnable,         // $C0EB
                             DiskReadWrite,      // $C0EC
                             DiskSetLatchValue,  // $C0ED
                             DiskSetReadMode,    // $C0EE
                             DiskSetWriteMode,   // $C0EF
                             HD_IO_EMUL,         // $C0F0
                             HD_IO_EMUL,         // $C0F1
                             HD_IO_EMUL,         // $C0F2
                             HD_IO_EMUL,         // $C0F3
                             HD_IO_EMUL,         // $C0F4
                             HD_IO_EMUL,         // $C0F5
                             HD_IO_EMUL,         // $C0F6
                             HD_IO_EMUL,         // $C0F7
                             HD_IO_EMUL,         // $C0F8
                             NullIo,             // $C0F9
                             NullIo,             // $C0FA
                             NullIo,             // $C0FB
                             NullIo,             // $C0FC
                             NullIo,             // $C0FD
                             NullIo,             // $C0FE
                             NullIo};            // $C0FF

iofunction iowrite[0x100] = {MemSetPaging,       // $C000
                             MemSetPaging,       // $C001
                             MemSetPaging,       // $C002
                             MemSetPaging,       // $C003
                             MemSetPaging,       // $C004
                             MemSetPaging,       // $C005
                             MemSetPaging,       // $C006
                             MemSetPaging,       // $C007
                             MemSetPaging,       // $C008
                             MemSetPaging,       // $C009
                             MemSetPaging,       // $C00A
                             MemSetPaging,       // $C00B
                             VideoSetMode,       // $C00C
                             VideoSetMode,       // $C00D
                             VideoSetMode,       // $C00E
                             VideoSetMode,       // $C00F
                             KeybReadFlag,       // $C010
                             KeybReadFlag,       // $C011
                             KeybReadFlag,       // $C012
                             KeybReadFlag,       // $C013
                             KeybReadFlag,       // $C014
                             KeybReadFlag,       // $C015
                             KeybReadFlag,       // $C016
                             KeybReadFlag,       // $C017
                             KeybReadFlag,       // $C018
                             KeybReadFlag,       // $C019
                             KeybReadFlag,       // $C01A
                             KeybReadFlag,       // $C01B
                             KeybReadFlag,       // $C01C
                             KeybReadFlag,       // $C01D
                             KeybReadFlag,       // $C01E
                             KeybReadFlag,       // $C01F
                             NullIo,             // $C020
                             NullIo,             // $C021
                             NullIo,             // $C022
                             NullIo,             // $C023
                             NullIo,             // $C024
                             NullIo,             // $C025
                             NullIo,             // $C026
                             NullIo,             // $C027
                             NullIo,             // $C028
                             NullIo,             // $C029
                             NullIo,             // $C02A
                             NullIo,             // $C02B
                             NullIo,             // $C02C
                             NullIo,             // $C02D
                             NullIo,             // $C02E
                             NullIo,             // $C02F
                             SpkrToggle,         // $C030
                             SpkrToggle,         // $C031
                             SpkrToggle,         // $C032
                             SpkrToggle,         // $C033
                             SpkrToggle,         // $C034
                             SpkrToggle,         // $C035
                             SpkrToggle,         // $C036
                             SpkrToggle,         // $C037
                             SpkrToggle,         // $C038
                             SpkrToggle,         // $C039
                             SpkrToggle,         // $C03A
                             SpkrToggle,         // $C03B
                             SpkrToggle,         // $C03C
                             SpkrToggle,         // $C03D
                             SpkrToggle,         // $C03E
                             SpkrToggle,         // $C03F
                             NullIo,             // $C040
                             NullIo,             // $C041
                             NullIo,             // $C042
                             NullIo,             // $C043
                             NullIo,             // $C044
                             NullIo,             // $C045
                             NullIo,             // $C046
                             NullIo,             // $C047
                             NullIo,             // $C048
                             NullIo,             // $C049
                             NullIo,             // $C04A
                             NullIo,             // $C04B
                             NullIo,             // $C04C
                             NullIo,             // $C04D
                             NullIo,             // $C04E
                             NullIo,             // $C04F
                             VideoSetMode,       // $C050
                             VideoSetMode,       // $C051
                             VideoSetMode,       // $C052
                             VideoSetMode,       // $C053
                             MemSetPaging,       // $C054
                             MemSetPaging,       // $C055
                             MemSetPaging,       // $C056
                             MemSetPaging,       // $C057
                             NullIo,             // $C058
                             NullIo,             // $C059
                             NullIo,             // $C05A
                             NullIo,             // $C05B
                             NullIo,             // $C05C
                             NullIo,             // $C05D
                             VideoSetMode,       // $C05E
                             VideoSetMode,       // $C05F
                             NullIo,             // $C060
                             NullIo,             // $C061
                             NullIo,             // $C062
                             NullIo,             // $C063
                             NullIo,             // $C064
                             NullIo,             // $C065
                             NullIo,             // $C066
                             NullIo,             // $C067
                             NullIo,             // $C068
                             NullIo,             // $C069
                             NullIo,             // $C06A
                             NullIo,             // $C06B
                             NullIo,             // $C06C
                             NullIo,             // $C06D
                             NullIo,             // $C06E
                             NullIo,             // $C06F
                             JoyResetPosition,   // $C070
#ifdef RAMWORKS
							 MemSetPaging,		 // $C071 - extended memory card set page
							 NullIo,			 // $C072
							 MemSetPaging,		 // $C073 - Ramworks III set page
#else
							 NullIo,			 // $C071
							 NullIo,			 // $C072
							 NullIo,			 // $C073
#endif
                             NullIo,             // $C074
                             NullIo,             // $C075
                             NullIo,             // $C076
                             NullIo,             // $C077
                             NullIo,             // $C078
                             NullIo,             // $C079
                             NullIo,             // $C07A
                             NullIo,             // $C07B
                             NullIo,             // $C07C
                             NullIo,             // $C07D
                             NullIo,             // $C07E
                             NullIo,             // $C07F
                             MemSetPaging,       // $C080
                             MemSetPaging,       // $C081
                             MemSetPaging,       // $C082
                             MemSetPaging,       // $C083
                             MemSetPaging,       // $C084
                             MemSetPaging,       // $C085
                             MemSetPaging,       // $C086
                             MemSetPaging,       // $C087
                             MemSetPaging,       // $C088
                             MemSetPaging,       // $C089
                             MemSetPaging,       // $C08A
                             MemSetPaging,       // $C08B
                             MemSetPaging,       // $C08C
                             MemSetPaging,       // $C08D
                             MemSetPaging,       // $C08E
                             MemSetPaging,       // $C08F
                             NullIo,             // $C090
                             NullIo,             // $C091
                             NullIo,             // $C092
                             NullIo,             // $C093
                             NullIo,             // $C094
                             NullIo,             // $C095
                             NullIo,             // $C096
                             NullIo,             // $C097
                             NullIo,             // $C098
                             NullIo,             // $C099
                             NullIo,             // $C09A
                             NullIo,             // $C09B
                             NullIo,             // $C09C
                             NullIo,             // $C09D
                             NullIo,             // $C09E
                             NullIo,             // $C09F
                             NullIo,             // $C0A0
                             NullIo,             // $C0A1
                             NullIo,             // $C0A2
                             NullIo,             // $C0A3
                             NullIo,             // $C0A4
                             NullIo,             // $C0A5
                             NullIo,             // $C0A6
                             NullIo,             // $C0A7
                             CommTransmit,       // $C0A8
                             CommStatus,         // $C0A9
                             CommCommand,        // $C0AA
                             CommControl,        // $C0AB
                             NullIo,             // $C0AC
                             NullIo,             // $C0AD
                             NullIo,             // $C0AE
                             NullIo,             // $C0AF
                             TfeIo,              // $C0B0
                             TfeIo,              // $C0B1
                             TfeIo,              // $C0B2
                             TfeIo,              // $C0B3
                             TfeIo,              // $C0B4
                             TfeIo,              // $C0B5
                             TfeIo,              // $C0B6
                             TfeIo,              // $C0B7
                             TfeIo,              // $C0B8
                             TfeIo,              // $C0B9
                             TfeIo,              // $C0BA
                             TfeIo,              // $C0BB
                             TfeIo,              // $C0BC
                             TfeIo,              // $C0BD
                             TfeIo,              // $C0BE
                             TfeIo,              // $C0BF
                             PhasorIO,           // $C0C0
                             PhasorIO,           // $C0C1
                             PhasorIO,           // $C0C2
                             PhasorIO,           // $C0C3
                             PhasorIO,           // $C0C4
                             PhasorIO,           // $C0C5
                             PhasorIO,           // $C0C6
                             PhasorIO,           // $C0C7
                             PhasorIO,           // $C0C8
                             PhasorIO,           // $C0C9
                             PhasorIO,           // $C0CA
                             PhasorIO,           // $C0CB
                             PhasorIO,           // $C0CC
                             PhasorIO,           // $C0CD
                             PhasorIO,           // $C0CE
                             PhasorIO,           // $C0CF
                             PhasorIO,           // $C0D0
                             PhasorIO,           // $C0D1
                             PhasorIO,           // $C0D2
                             PhasorIO,           // $C0D3
                             PhasorIO,           // $C0D4
                             PhasorIO,           // $C0D5
                             PhasorIO,           // $C0D6
                             PhasorIO,           // $C0D7
                             PhasorIO,           // $C0D8
                             PhasorIO,           // $C0D9
                             PhasorIO,           // $C0DA
                             PhasorIO,           // $C0DB
                             PhasorIO,           // $C0DC
                             PhasorIO,           // $C0DD
                             PhasorIO,           // $C0DE
                             PhasorIO,           // $C0DF
                             DiskControlStepper, // $C0E0
                             DiskControlStepper, // $C0E1
                             DiskControlStepper, // $C0E2
                             DiskControlStepper, // $C0E3
                             DiskControlStepper, // $C0E4
                             DiskControlStepper, // $C0E5
                             DiskControlStepper, // $C0E6
                             DiskControlStepper, // $C0E7
                             DiskControlMotor,   // $C0E8
                             DiskControlMotor,   // $C0E9
                             DiskEnable,         // $C0EA
                             DiskEnable,         // $C0EB
                             DiskReadWrite,      // $C0EC
                             DiskSetLatchValue,  // $C0ED
                             DiskSetReadMode,    // $C0EE
                             DiskSetWriteMode,   // $C0EF
                             HD_IO_EMUL,         // $C0F0
                             HD_IO_EMUL,         // $C0F1
                             HD_IO_EMUL,         // $C0F2
                             HD_IO_EMUL,         // $C0F3
                             HD_IO_EMUL,         // $C0F4
                             HD_IO_EMUL,         // $C0F5
                             HD_IO_EMUL,         // $C0F6
                             HD_IO_EMUL,         // $C0F7
                             HD_IO_EMUL,         // $C0F8
                             NullIo,             // $C0F9
                             NullIo,             // $C0FA
                             NullIo,             // $C0FB
                             NullIo,             // $C0FC
                             NullIo,             // $C0FD
                             NullIo,             // $C0FE
                             NullIo};            // $C0FF

static DWORD   imagemode[MAXIMAGES];
LPBYTE         memshadow[MAXIMAGES][0x100];
LPBYTE         memwrite[MAXIMAGES][0x100];

static BOOL    fastpaging   = 0;
DWORD          image        = 0;
DWORD          lastimage    = 0;
static BOOL    lastwriteram = 0;
LPBYTE         mem          = NULL;
static LPBYTE  memaux       = NULL;
LPBYTE         memdirty     = NULL;
static LPBYTE  memimage     = NULL;
static LPBYTE  memmain      = NULL;
static DWORD   memmode      = MF_BANK2 | MF_SLOTCXROM | MF_WRITERAM;
static LPBYTE  memrom       = NULL;
static BOOL    modechanging = 0;
DWORD          pages        = 0;

MemoryInitPattern_e g_eMemoryInitPattern = MIP_FF_FF_00_00;

#ifdef RAMWORKS
UINT			g_uMaxExPages	= 1;			// user requested ram pages
static LPBYTE	RWpages[128];					// pointers to RW memory banks
#endif

void UpdatePaging (BOOL initialize, BOOL updatewriteonly);

//===========================================================================
void BackMainImage () {
  int loop = 0;
  for (loop = 0; loop < 256; loop++) {
    if (memshadow[0][loop] &&
        ((*(memdirty+loop) & 1) || (loop <= 1)))
      CopyMemory(memshadow[0][loop],memimage+(loop << 8),256);
    *(memdirty+loop) &= ~1;
  }
}

//===========================================================================
BYTE __stdcall NullIo (WORD programcounter, BYTE address, BYTE write, BYTE value, ULONG nCycles) {
	if (!write)
	{
		return MemReadFloatingBus();
	}
	else
	{
		return 0;
	}
}

//===========================================================================
void ResetPaging (BOOL initialize) {
  if (!initialize)
    MemSetFastPaging(0);
  lastwriteram = 0;
  memmode      = MF_BANK2 | MF_SLOTCXROM | MF_WRITERAM;
  UpdatePaging(initialize,0);
}

//===========================================================================
void UpdateFastPaging () {
  BOOL  found    = 0;
  DWORD imagenum = 0;
  do
    if ((imagemode[imagenum] == memmode) ||
        ((lastimage >= 3) &&
         ((imagemode[imagenum] & MF_IMAGEMASK) == (memmode & MF_IMAGEMASK))))
      found = 1;
    else
      ++imagenum;
  while ((imagenum <= lastimage) && !found);
  if (found) {
    image = imagenum;
    mem   = memimage+(image << 16);
    if (imagemode[image] != memmode) {
      imagemode[image] = memmode;
      UpdatePaging(0,1);
    }
  }
  else {
    if (lastimage < MAXIMAGES-1) {
      imagenum = ++lastimage;
      if (lastimage >= 3)
        VirtualAlloc(memimage+lastimage*0x10000,0x10000,MEM_COMMIT,PAGE_READWRITE);
    }
    else {
      static DWORD nextimage = 0;
      if (nextimage > lastimage)
        nextimage = 0;
      imagenum = nextimage++;
    }
    imagemode[image = imagenum] = memmode;
    mem = memimage+(image << 16);
    UpdatePaging(1,0);
  }
  CpuReinitialize();
}

//===========================================================================
void UpdatePaging (BOOL initialize, BOOL updatewriteonly) {

  // SAVE THE CURRENT PAGING SHADOW TABLE
  LPBYTE oldshadow[256];
  if (!(initialize || fastpaging || updatewriteonly))
    CopyMemory(oldshadow,memshadow[image],256*sizeof(LPBYTE));

  // UPDATE THE PAGING TABLES BASED ON THE NEW PAGING SWITCH VALUES
  int loop;
  if (initialize) {
    for (loop = 0; loop < 192; loop++)
      memwrite[image][loop] = mem+(loop << 8);
    for (loop = 192; loop < 208; loop++)	// TC: [0xC000..0xCF00]
      memwrite[image][loop] = NULL;
  }
  if (!updatewriteonly)
    for (loop = 0; loop < 2; loop++)
      memshadow[image][loop] = SW_ALTZP ? memaux+(loop << 8) : memmain+(loop << 8);
  for (loop = 2; loop < 192; loop++) {
    memshadow[image][loop] = SW_AUXREAD ? memaux+(loop << 8)
                                        : memmain+(loop << 8);
    memwrite[image][loop]  = ((SW_AUXREAD != 0) == (SW_AUXWRITE != 0))
                               ? mem+(loop << 8)
                               : SW_AUXWRITE ? memaux+(loop << 8)
                                             : memmain+(loop << 8);
  }
  if (!updatewriteonly) {
    for (loop = 192; loop < 200; loop++)
      if (loop == 195)
        memshadow[image][loop] = (SW_SLOTC3ROM && SW_SLOTCXROM) ? memrom+0x0300
                                                                : memrom+0x1300;
      else
        memshadow[image][loop] = SW_SLOTCXROM ? memrom+(loop << 8)-0xC000
                                              : memrom+(loop << 8)-0xB000;
    for (loop = 200; loop < 208; loop++)
      memshadow[image][loop] = memrom+(loop << 8)-0xB000;
  }
  for (loop = 208; loop < 224; loop++) {
    int bankoffset = (SW_BANK2 ? 0 : 0x1000);
    memshadow[image][loop] = SW_HIGHRAM ? SW_ALTZP ? memaux+(loop << 8)-bankoffset
                                                   : memmain+(loop << 8)-bankoffset
                                        : memrom+(loop << 8)-0xB000;
    memwrite[image][loop]  = SW_WRITERAM ? SW_HIGHRAM ? mem+(loop << 8)
                                                      : SW_ALTZP ? memaux+(loop << 8)-bankoffset
                                                                 : memmain+(loop << 8)-bankoffset
                                         : NULL;
  }
  for (loop = 224; loop < 256; loop++) {
    memshadow[image][loop] = SW_HIGHRAM ? SW_ALTZP ? memaux+(loop << 8)
                                                   : memmain+(loop << 8)
                                        : memrom+(loop << 8)-0xB000;
    memwrite[image][loop]  = SW_WRITERAM ? SW_HIGHRAM ? mem+(loop << 8)
                                                      : SW_ALTZP ? memaux+(loop << 8)
                                                                 : memmain+(loop << 8)
                                         : NULL;
  }
  if (SW_80STORE) {
    for (loop = 4; loop < 8; loop++) {
      memshadow[image][loop] = SW_PAGE2 ? memaux+(loop << 8)
                                        : memmain+(loop << 8);
      memwrite[image][loop]  = mem+(loop << 8);
    }
    if (SW_HIRES)
      for (loop = 32; loop < 64; loop++) {
        memshadow[image][loop] = SW_PAGE2 ? memaux+(loop << 8)
                                          : memmain+(loop << 8);
        memwrite[image][loop]  = mem+(loop << 8);
      }
  }

  // MOVE MEMORY BACK AND FORTH AS NECESSARY BETWEEN THE SHADOW AREAS AND
  // THE MAIN RAM IMAGE TO KEEP BOTH SETS OF MEMORY CONSISTENT WITH THE NEW
  // PAGING SHADOW TABLE
  if (!updatewriteonly)
    for (loop = 0; loop < 256; loop++)
      if (initialize || (oldshadow[loop] != memshadow[image][loop])) {
        if ((!(initialize || fastpaging)) &&
            ((*(memdirty+loop) & 1) || (loop <= 1))) {
          *(memdirty+loop) &= ~1;
          CopyMemory(oldshadow[loop],mem+(loop << 8),256);
        }
        CopyMemory(mem+(loop << 8),memshadow[image][loop],256);
      }

}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BYTE __stdcall MemCheckPaging (WORD, BYTE address, BYTE, BYTE, ULONG) {
  BOOL result = 0;
  switch (address) {
    case 0x11: result = SW_BANK2;       break;
    case 0x12: result = SW_HIGHRAM;     break;
    case 0x13: result = SW_AUXREAD;     break;
    case 0x14: result = SW_AUXWRITE;    break;
    case 0x15: result = !SW_SLOTCXROM;  break;
    case 0x16: result = SW_ALTZP;       break;
    case 0x17: result = SW_SLOTC3ROM;   break;
    case 0x18: result = SW_80STORE;     break;
    case 0x1C: result = SW_PAGE2;       break;
    case 0x1D: result = SW_HIRES;       break;
  }
  return KeybGetKeycode() | (result ? 0x80 : 0);
}

//===========================================================================
void MemDestroy () {
  if (fastpaging)
    MemSetFastPaging(0);
  VirtualFree(memimage,MAX(0x30000,0x10000*(lastimage+1)),MEM_DECOMMIT);
  VirtualFree(memaux  ,0,MEM_RELEASE);
  VirtualFree(memdirty,0,MEM_RELEASE);
  VirtualFree(memimage,0,MEM_RELEASE);
  VirtualFree(memmain ,0,MEM_RELEASE);
  VirtualFree(memrom  ,0,MEM_RELEASE);
#ifdef RAMWORKS
	for (UINT i=1; i<g_uMaxExPages; i++)
	{
		if (RWpages[i])
		{
			VirtualFree(RWpages[i], 0, MEM_RELEASE);
			RWpages[i] = NULL;
		}
	}
	RWpages[0]=NULL;
#endif
  memaux   = NULL;
  memdirty = NULL;
  memimage = NULL;
  memmain  = NULL;
  memrom   = NULL;
  mem      = NULL;
  ZeroMemory(memwrite, sizeof(memwrite));
  ZeroMemory(memshadow,sizeof(memshadow));
}

//===========================================================================

bool MemGet80Store()
{
	return SW_80STORE != 0;
}

//===========================================================================
LPBYTE MemGetAuxPtr (WORD offset)
{
	LPBYTE lpMem = (memshadow[image][(offset >> 8)] == (memaux+(offset & 0xFF00)))
			? mem+offset
			: memaux+offset;

#ifdef RAMWORKS
	if ( ((SW_PAGE2 && SW_80STORE) || VideoGetSW80COL()) &&
		( ( ((offset & 0xFF00)>=0x0400) &&
		((offset & 0xFF00)<=0700) ) ||
		( SW_HIRES && ((offset & 0xFF00)>=0x2000) &&
		((offset & 0xFF00)<=0x3F00) ) ) ) {
		lpMem = (memshadow[image][(offset >> 8)] == (RWpages[0]+(offset & 0xFF00)))
			? mem+offset
			: RWpages[0]+offset;
	}
#endif

	return lpMem;
}

//===========================================================================
LPBYTE MemGetMainPtr (WORD offset) {
  return (memshadow[image][(offset >> 8)] == (memmain+(offset & 0xFF00)))
           ? mem+offset
           : memmain+offset;
}

//===========================================================================
void MemInitialize () {

  // ALLOCATE MEMORY FOR THE APPLE MEMORY IMAGE AND ASSOCIATED DATA
  // STRUCTURES
  //
  // THE MEMIMAGE BUFFER CAN CONTAIN EITHER MULTIPLE MEMORY IMAGES OR
  // ONE MEMORY IMAGE WITH COMPILER DATA
  memaux   = (LPBYTE)VirtualAlloc(NULL,0x10000,MEM_COMMIT,PAGE_READWRITE);
  memdirty = (LPBYTE)VirtualAlloc(NULL,0x100  ,MEM_COMMIT,PAGE_READWRITE);
  memmain  = (LPBYTE)VirtualAlloc(NULL,0x10000,MEM_COMMIT,PAGE_READWRITE);
  memrom   = (LPBYTE)VirtualAlloc(NULL,0x5000 ,MEM_COMMIT,PAGE_READWRITE);
  memimage = (LPBYTE)VirtualAlloc(NULL,
                                  MAX(0x30000,MAXIMAGES*0x10000),
                                  MEM_RESERVE,PAGE_NOACCESS);
  if ((!memaux) || (!memdirty) || (!memimage) || (!memmain) || (!memrom)) {
    MessageBox(GetDesktopWindow(),
               TEXT("The emulator was unable to allocate the memory it ")
               TEXT("requires.  Further execution is not possible."),
               TITLE,
               MB_ICONSTOP | MB_SETFOREGROUND);
    ExitProcess(1);
  }
  {
    LPVOID newloc = VirtualAlloc(memimage,0x30000,MEM_COMMIT,PAGE_READWRITE);
    if (newloc != memimage)
      MessageBox(GetDesktopWindow(),
                 TEXT("The emulator has detected a bug in your operating ")
                 TEXT("system.  While changing the attributes of a memory ")
                 TEXT("object, the operating system also changed its ")
                 TEXT("location."),
                 TITLE,
                 MB_ICONEXCLAMATION | MB_SETFOREGROUND);
  }

#ifdef RAMWORKS
	// allocate memory for RAMWorks III - up to 8MB
	RWpages[0] = memaux;
	UINT i = 1;
	while ((i < g_uMaxExPages) && (RWpages[i] = (LPBYTE) VirtualAlloc(NULL,0x10000,MEM_COMMIT,PAGE_READWRITE)))
		i++;
#endif

  // READ THE APPLE FIRMWARE ROMS INTO THE ROM IMAGE
	const UINT ROM_SIZE = 0x5000; // HACK: Magic #

	HRSRC hResInfo = apple2e	? FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2E_ROM), "ROM")
	                                : (apple2plus ? FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2PLUS_ROM), "ROM")
					              : FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2ORIG_ROM), "ROM"));
	if(hResInfo == NULL)
	{
		TCHAR sRomFileName[ 128 ];
		_tcscpy( sRomFileName, apple2e ? TEXT("APPLE2E.ROM")
					       : (apple2plus ? TEXT("APPLE2PLUS.ROM")
					                     : TEXT("APPLE2ORIG.ROM")));

		TCHAR sText[ 256 ];
		wsprintf( sText, TEXT("Unable to open the required firmware ROM data file.\n\nFile: %s."), sRomFileName );

		MessageBox(GetDesktopWindow(),
               sText,
               TITLE,
               MB_ICONSTOP | MB_SETFOREGROUND);
		ExitProcess(1);
	}

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != ROM_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	memcpy(memrom, pData, ROM_SIZE);

  // TODO/FIXME: HACK! REMOVE A WAIT ROUTINE FROM THE DISK CONTROLLER'S FIRMWARE
  {
    *(memrom+0x064C) = 0xA9;
    *(memrom+0x064D) = 0x00;
    *(memrom+0x064E) = 0xEA;
  }

  HD_Load_Rom(memrom);	// HDD f/w gets loaded to $C700

  MemReset();
}

//===========================================================================

// Call by:
// . ResetMachineState()	eg. Power-cycle ('Apple-Go' button)
// . Snapshot_LoadState()
void MemReset ()
{
  // TURN OFF FAST PAGING IF IT IS CURRENTLY ACTIVE
  MemSetFastPaging(0);

  // INITIALIZE THE PAGING TABLES
  ZeroMemory(memshadow,MAXIMAGES*256*sizeof(LPBYTE));
  ZeroMemory(memwrite ,MAXIMAGES*256*sizeof(LPBYTE));

  // INITIALIZE THE RAM IMAGES
  ZeroMemory(memaux ,0x10000);

  ZeroMemory(memmain,0x10000);

	int iByte;

	if (g_eMemoryInitPattern == MIP_FF_FF_00_00)
	{
		for( iByte = 0x0000; iByte < 0xC000; )
		{
			memmain[ iByte++ ] = 0xFF;
			memmain[ iByte++ ] = 0xFF;

			iByte++;
			iByte++;
		}
	}

  // SET UP THE MEMORY IMAGE
  mem   = memimage;
  image = 0;

  // INITIALIZE THE CPU
  CpuInitialize();

  // INITIALIZE PAGING, FILLING IN THE 64K MEMORY IMAGE
  ResetPaging(1);
  regs.bRESET = 1;
}

//===========================================================================

// Call by:
// . Soft-reset (Ctrl+Reset)
// . Snapshot_LoadState()
void MemResetPaging ()
{
  ResetPaging(0);
}

//===========================================================================
BYTE MemReturnRandomData (BYTE highbit) {
  static const BYTE retval[16] = {0x00,0x2D,0x2D,0x30,0x30,0x32,0x32,0x34,
                                  0x35,0x39,0x43,0x43,0x43,0x60,0x7F,0x7F};
  BYTE r = (BYTE)(rand() & 0xFF);
  if (r <= 170)
    return 0x20 | (highbit ? 0x80 : 0);
  else
    return retval[r & 15] | (highbit ? 0x80 : 0);
}

//===========================================================================

BYTE MemReadFloatingBus()
{
  return*(LPBYTE)(mem + VideoGetScannerAddress());
}

//===========================================================================

BYTE MemReadFloatingBus(BYTE const highbit)
{
  BYTE r = *(LPBYTE)(mem + VideoGetScannerAddress());
  return (r & ~0x80) | ((highbit) ? 0x80 : 0);
}

//===========================================================================
void MemSetFastPaging (BOOL on) {
  if (fastpaging && modechanging) {
    modechanging = 0;
    UpdateFastPaging();
  }
  else if (!fastpaging) {
    BackMainImage();
    if (lastimage >= 3)
      VirtualFree(memimage+0x30000,(lastimage-2) << 16,MEM_DECOMMIT);
  }
  fastpaging   = on;
  image        = 0;
  mem          = memimage;
  lastimage    = 0;
  imagemode[0] = memmode;
  if (!fastpaging)
    UpdatePaging(1,0);
  cpuemtype = fastpaging ? CPU_FASTPAGING : CPU_COMPILING;
  CpuReinitialize();
  if (cpuemtype == CPU_COMPILING)
    CpuResetCompilerData();
}

//===========================================================================
BYTE __stdcall MemSetPaging (WORD programcounter, BYTE address, BYTE write, BYTE value, ULONG) {
  DWORD lastmemmode = memmode;

  // DETERMINE THE NEW MEMORY PAGING MODE.
  if ((address >= 0x80) && (address <= 0x8F))
  {
    BOOL writeram = (address & 1);
    memmode &= ~(MF_BANK2 | MF_HIGHRAM | MF_WRITERAM);
    lastwriteram = 1; // note: because diags.do doesn't set switches twice!
    if (lastwriteram && writeram)
      memmode |= MF_WRITERAM;
    if (!(address & 8))
      memmode |= MF_BANK2;
    if (((address & 2) >> 1) == (address & 1))
      memmode |= MF_HIGHRAM;
    lastwriteram = writeram;
  }
  else if (apple2e)
  {
    switch (address)
	{
		case 0x00: memmode &= ~MF_80STORE;    break;
		case 0x01: memmode |=  MF_80STORE;    break;
		case 0x02: memmode &= ~MF_AUXREAD;    break;
		case 0x03: memmode |=  MF_AUXREAD;    break;
		case 0x04: memmode &= ~MF_AUXWRITE;   break;
		case 0x05: memmode |=  MF_AUXWRITE;   break;
		case 0x06: memmode |=  MF_SLOTCXROM;  break;
		case 0x07: memmode &= ~MF_SLOTCXROM;  break;
		case 0x08: memmode &= ~MF_ALTZP;      break;
		case 0x09: memmode |=  MF_ALTZP;      break;
		case 0x0A: memmode &= ~MF_SLOTC3ROM;  break;
		case 0x0B: memmode |=  MF_SLOTC3ROM;  break;
		case 0x54: memmode &= ~MF_PAGE2;      break;
		case 0x55: memmode |=  MF_PAGE2;      break;
		case 0x56: memmode &= ~MF_HIRES;      break;
		case 0x57: memmode |=  MF_HIRES;      break;
#ifdef RAMWORKS
		case 0x71: // extended memory aux page number
		case 0x73: // Ramworks III set aux page number
			if ((value < g_uMaxExPages) && RWpages[value])
			{
				memaux = RWpages[value];
				//memmode &= ~MF_RWPMASK;
				//memmode |= value;
				if (fastpaging)
				{
					UpdateFastPaging();
				}
				else
				{
					UpdatePaging(0,0);
					if (cpuemtype == CPU_COMPILING)
						CpuResetCompilerData();
				}
			}
			break;
#endif
	}
  }

  // IF THE EMULATED PROGRAM HAS JUST UPDATE THE MEMORY WRITE MODE AND IS
  // ABOUT TO UPDATE THE MEMORY READ MODE, HOLD OFF ON ANY PROCESSING UNTIL
  // IT DOES SO.
  if ((address >= 4) && (address <= 5) &&
      ((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0028D)) {
    modechanging = 1;
    return write ? 0 : MemReadFloatingBus(1);
  }
  if ((address >= 0x80) && (address <= 0x8F) && (programcounter < 0xC000) &&
      (((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0048D) ||
       ((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0028D))) {
    modechanging = 1;
    return write ? 0 : MemReadFloatingBus(1);
  }

  // IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
  // WRITE TABLES.
  if ((lastmemmode != memmode) || modechanging) {
    modechanging = 0;
    ++pages;

    // IF FAST PAGING IS ACTIVE, WE KEEP MULTIPLE COMPLETE MEMORY IMAGES
    // AND WRITE TABLES, AND SWITCH BETWEEN THEM.  THE FAST PAGING VERSION
    // OF THE CPU EMULATOR KEEPS ALL OF THE IMAGES COHERENT.
    if (fastpaging)
      UpdateFastPaging();

    // IF FAST PAGING IS NOT ACTIVE THEN WE KEEP ONLY ONE MEMORY IMAGE AND
    // WRITE TABLE, AND UPDATE THEM EVERY TIME PAGING IS CHANGED.
    else {
      UpdatePaging(0,0);
      if (cpuemtype == CPU_COMPILING)
        CpuResetCompilerData();
    }

  }

  if ((address <= 1) || ((address >= 0x54) && (address <= 0x57)))
    return VideoSetMode(programcounter,address,write,value,0);

  return write ? 0 : MemReadFloatingBus();
}

//===========================================================================
void MemTrimImages () {
  if (fastpaging && (lastimage > 2)) {
    if (modechanging) {
      modechanging = 0;
      UpdateFastPaging();
    }
    static DWORD trimnumber = 0;
    if ((image != trimnumber) &&
        (image != lastimage) &&
        (trimnumber < lastimage)) {
      imagemode[trimnumber] = imagemode[lastimage];
      VirtualFree(memimage+(lastimage-- << 16),0x10000,MEM_DECOMMIT);
      DWORD realimage = image;
      image   = trimnumber;
      mem     = memimage+(image << 16);
      memmode = imagemode[image];
      UpdatePaging(1,0);
      image   = realimage;
      mem     = memimage+(image << 16);
      memmode = imagemode[image];
      CpuReinitialize();
    }
    if (++trimnumber >= lastimage)
      trimnumber = 0;
  }
}

//===========================================================================

BYTE __stdcall CxReadFunc(WORD, WORD nAddr, BYTE, BYTE, ULONG nCyclesLeft)
{
	USHORT nPage = nAddr>>8;	// Don't use BYTE - Bug in VC++ 6.0 (SP5)!

	CpuCalcCycles(nCyclesLeft);
	
	if(!apple2e || SW_SLOTCXROM)
	{
		if((nPage == 0xC4) || (nPage == 0xC5))
		{
			// Slot 4 or 5: Mockingboard
			return MB_Read(nAddr);
		}
		else
		{
			return mem[nAddr];
		}
	}
	else
	{
#if _DEBUG
		// Gets triggered by opcode $29 (IMM AND) by internal emulation code
//		if((nPage == 0xC4) || (nPage == 0xC5))
//			_ASSERT(0);
#endif
		return mem[nAddr];
	}
}

BYTE __stdcall CxWriteFunc(WORD, WORD nAddr, BYTE, BYTE nValue, ULONG nCyclesLeft)
{
	BYTE nPage = nAddr>>8;

	CpuCalcCycles(nCyclesLeft);

	if(!apple2e || SW_SLOTCXROM)
	{
		if((nPage == 0xC4) || (nPage == 0xC5))
		{
			// Slot 4 or 5: Mockingboard
			MB_Write(nAddr, nValue);
		}
	}
#if _DEBUG
	else
	{
		if((nPage == 0xC4) || (nPage == 0xC5))
			_ASSERT(0);
	}
#endif

	return 0;
}

//===========================================================================

DWORD MemGetSnapshot(SS_BaseMemory* pSS)
{
	pSS->dwMemMode = memmode;
	pSS->bLastWriteRam = lastwriteram;

	for(DWORD dwOffset = 0x0000; dwOffset < 0x10000; dwOffset+=0x100)
	{
		memcpy(pSS->nMemMain+dwOffset, MemGetMainPtr((WORD)dwOffset), 0x100);
		memcpy(pSS->nMemAux+dwOffset, MemGetAuxPtr((WORD)dwOffset), 0x100);
	}

	return 0;
}

DWORD MemSetSnapshot(SS_BaseMemory* pSS)
{
	memmode = pSS->dwMemMode;
	lastwriteram = pSS->bLastWriteRam;

	memcpy(memmain, pSS->nMemMain, nMemMainSize);
	memcpy(memaux, pSS->nMemAux, nMemAuxSize);

	//

	pages = 0;
	modechanging = 0;

	UpdatePaging(1,0);		// Initialize=1, UpdateWriteOnly=0

	return 0;
}
