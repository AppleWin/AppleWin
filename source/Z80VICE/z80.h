/*
 * z80.h
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef _Z80_H
#define _Z80_H

struct z80_regs_s;

extern struct z80_regs_s z80_regs;

//struct interrupt_cpu_status_s;
//struct alarm_context_s;

extern void z80_reset();
//extern void z80_mainloop(struct interrupt_cpu_status_s *cpu_int_status,
//                         struct alarm_context_s *cpu_alarm_context);
DWORD z80_mainloop(ULONG uTotalCycles, ULONG uExecutedCycles);
//extern void z80_trigger_dma();

BYTE z80_RDMEM(WORD Addr);
void z80_WRMEM(WORD Addr, BYTE Value);

// The board the (single) Z80 sits on. None installed: Microsoft's Z-80
// SoftCard, as before (fixed map, 2 T-states per 6502 cycle, no interrupts).
// A card that brings its own memory, I/O ports, clock and /INT installs one.
struct Z80Board
{
	BYTE (*read)(WORD addr, ULONG cycles);              // cycles: 6502 cycles into this burst
	void (*write)(WORD addr, BYTE value, ULONG cycles);
	BYTE (*in)(WORD port);
	void (*out)(WORD port, BYTE value);
	bool (*intLine)(ULONG cycles);                      // /INT asserted (a level)
	BYTE (*intAck)();                                   // the data bus byte during the acknowledge
	bool (*halfSpeed)();                                // clock runs at half rate now
	bool (*m1Stretch)();                                // every M1 cycle is half a T-state longer now
	double tStatesPer6502Cycle;                         // at full rate
};
void Z80_SetBoard(const Z80Board* board);

// The registers alone, for a card that saves its own snapshot.
void Z80_SaveRegisters(class YamlSaveHelper& yamlSaveHelper);
void Z80_LoadRegisters(class YamlLoadHelper& yamlLoadHelper);

const std::string& Z80_GetSnapshotCardNameOld();
const std::string& Z80_GetSnapshotCardName();
void Z80_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const UINT uSlot);
bool Z80_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT uSlot, UINT version);

#endif
