/*
 * z80mem.h
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

#ifndef _Z80MEM_H
#define _Z80MEM_H

#ifdef WATCOM_COMPILE
#include "../mem.h"
#else
#include "../CommonVICE/mem.h"		// [AppleWin-TC]
#endif

#include "../CommonVICE/types.h"	// [AppleWin-TC]

extern int z80mem_resources_init(void);
extern int z80mem_cmdline_options_init(void);

extern void z80mem_set_bank_pointer(BYTE **base, int *limit);
extern void z80mem_update_config(int config);

extern int z80mem_load(void);
extern BYTE z80bios_rom[0x1000];

extern void z80mem_initialize(void);

/* Pointers to the currently used memory read and write tables.  */
extern read_func_ptr_t *_z80mem_read_tab_ptr;
extern store_func_ptr_t *_z80mem_write_tab_ptr;
extern BYTE **_z80mem_read_base_tab_ptr;
extern int *z80mem_read_limit_tab_ptr;

extern BYTE REGPARM1 bios_read(WORD addr);
extern void REGPARM2 bios_store(WORD addr, BYTE value);

extern store_func_ptr_t io_write_tab[];
extern read_func_ptr_t io_read_tab[];

extern unsigned int z80_old_reg_pc;

#endif

