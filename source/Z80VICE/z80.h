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

extern void z80_reset(void);
//extern void z80_mainloop(struct interrupt_cpu_status_s *cpu_int_status,
//                         struct alarm_context_s *cpu_alarm_context);
DWORD z80_mainloop(ULONG uTotalCycles, ULONG uExecutedCycles);
//extern void z80_trigger_dma(void);

BYTE z80_RDMEM(WORD Addr);
void z80_WRMEM(WORD Addr, BYTE Value);

#endif

