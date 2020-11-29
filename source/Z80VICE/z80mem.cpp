/*
 * z80mem.c
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

//#include "vice.h"

#include "StdAfx.h"

//#include "c128mem.h"					// [AppleWin-TC]
//#include "c128mmu.h"
//#include "c64cia.h"
//#include "c64io.h"
//#include "cmdline.h"
//#include "log.h"
//#include "mem.h"
//#include "resources.h"
//#include "sid.h"
//#include "sysfile.h"
//#include "types.h"
//#include "vdc-mem.h"
//#include "vdc.h"
//#include "vicii-mem.h"
//#include "vicii.h"
#include "../CommonVICE/types.h"		// [AppleWin-TC]
#include "z80mem.h"
#include "z80.h"						// [AppleWin-TC] Added for z80_RDMEM() & z80_WRMEM


/* Z80 boot BIOS.  */
BYTE z80bios_rom[0x1000];

/* Logging.  */
//static log_t z80mem_log = LOG_ERR;	//	[AppleWin-TC]

/* Adjust this pointer when the MMU changes banks.  */
static BYTE **bank_base;
static int *bank_limit = NULL;
unsigned int z80_old_reg_pc;

/* Pointers to the currently used memory read and write tables.  */
read_func_ptr_t *_z80mem_read_tab_ptr;
store_func_ptr_t *_z80mem_write_tab_ptr;
BYTE **_z80mem_read_base_tab_ptr;
int *z80mem_read_limit_tab_ptr;

#define NUM_CONFIGS 8

/* Memory read and write tables.  */
static store_func_ptr_t mem_write_tab[NUM_CONFIGS][0x101];
static read_func_ptr_t mem_read_tab[NUM_CONFIGS][0x101];
static BYTE *mem_read_base_tab[NUM_CONFIGS][0x101];
static int mem_read_limit_tab[NUM_CONFIGS][0x101];

store_func_ptr_t io_write_tab[0x101];
read_func_ptr_t io_read_tab[0x101];

//static const resource_int_t resources_int[] = {	// [AppleWin-TC]
//    { NULL }
//};
//
//int z80mem_resources_init(void)
//{
//    return resources_register_int(resources_int);
//}
//
//static const cmdline_option_t cmdline_options[] =
//{
//    { NULL }
//};
//
//int z80mem_cmdline_options_init(void)
//{
//    return cmdline_register_options(cmdline_options);
//}

/* ------------------------------------------------------------------------- */

/* Generic memory access.  */
#if 0
static void REGPARM2 z80mem_store(WORD addr, BYTE value)
{
    _z80mem_write_tab_ptr[addr >> 8](addr, value);
}

static BYTE REGPARM1 z80mem_read(WORD addr)
{
    return _z80mem_read_tab_ptr[addr >> 8](addr);
}
#endif

BYTE REGPARM1 bios_read(WORD addr)
{
    return z80bios_rom[addr & 0x0fff];
}

void REGPARM2 bios_store(WORD addr, BYTE value)
{
    z80bios_rom[addr] = value;
}

//static BYTE REGPARM1 z80_read_zero(WORD addr)	// [AppleWin-TC]
//{
//    return mem_page_zero[addr];
//}
//
//static void REGPARM2 z80_store_zero(WORD addr, BYTE value)
//{
//    mem_page_zero[addr] = value;
//}

static BYTE REGPARM1 read_unconnected_io(WORD addr)
{
//    log_message(z80mem_log, "Read from unconnected IO %04x", addr);				// [AppleWin-TC]
    return 0;
}

static void REGPARM2 store_unconnected_io(WORD addr, BYTE value)
{
//    log_message(z80mem_log, "Store to unconnected IO %04x %02x", addr, value);	// [AppleWin-TC]
}

#ifdef _MSC_VER
#pragma optimize("",off)
#endif

void z80mem_initialize(void)
{
	int i, j;

    /* Memory addess space.  */

    for (j = 0; j < NUM_CONFIGS; j++) {
        for (i = 0; i <= 0x100; i++) {
            mem_read_base_tab[j][i] = NULL;
            mem_read_limit_tab[j][i] = -1;

            mem_read_tab[j][i] = z80_RDMEM;		// [AppleWin-TC]
            mem_write_tab[j][i] = z80_WRMEM;	// [AppleWin-TC]
        }
	}

    _z80mem_read_tab_ptr = mem_read_tab[0];
    _z80mem_write_tab_ptr = mem_write_tab[0];
    _z80mem_read_base_tab_ptr = mem_read_base_tab[0];
    z80mem_read_limit_tab_ptr = mem_read_limit_tab[0];

    /* IO address space.  */

    /* At least we know what happens.  */
    for (i = 0; i <= 0x100; i++) {
        io_read_tab[i] = read_unconnected_io;
        io_write_tab[i] = store_unconnected_io;
    }

#if 0	// [AppleWin-TC]
	int i, j;

    /* Memory addess space.  */

    for (j = 0; j < NUM_CONFIGS; j++) {
        for (i = 0; i <= 0x100; i++) {
            mem_read_base_tab[j][i] = NULL;
            mem_read_limit_tab[j][i] = -1;
        }
    }

    mem_read_tab[0][0] = bios_read;
    mem_write_tab[0][0] = z80_store_zero;
    mem_read_tab[1][0] = bios_read;
    mem_write_tab[1][0] = z80_store_zero;
    mem_read_tab[2][0] = z80_read_zero;
    mem_write_tab[2][0] = z80_store_zero;
    mem_read_tab[3][0] = z80_read_zero;
    mem_write_tab[3][0] = z80_store_zero;
    mem_read_tab[4][0] = z80_read_zero;
    mem_write_tab[4][0] = z80_store_zero;
    mem_read_tab[5][0] = z80_read_zero;
    mem_write_tab[5][0] = z80_store_zero;
    mem_read_tab[6][0] = z80_read_zero;
    mem_write_tab[6][0] = z80_store_zero;
    mem_read_tab[7][0] = z80_read_zero;
    mem_write_tab[7][0] = z80_store_zero;
 
    mem_read_tab[0][1] = bios_read;
    mem_write_tab[0][1] = one_store;
    mem_read_tab[1][1] = bios_read;
    mem_write_tab[1][1] = one_store;
    mem_read_tab[2][1] = one_read;
    mem_write_tab[2][1] = one_store;
    mem_read_tab[3][1] = one_read;
    mem_write_tab[3][1] = one_store;
    mem_read_tab[4][1] = one_read;
    mem_write_tab[4][1] = one_store;
    mem_read_tab[5][1] = one_read;
    mem_write_tab[5][1] = one_store;
    mem_read_tab[6][1] = one_read;
    mem_write_tab[6][1] = one_store;
    mem_read_tab[7][1] = one_read;
    mem_write_tab[7][1] = one_store;

    for (i = 2; i < 0x10; i++) {
        mem_read_tab[0][i] = bios_read;
        mem_write_tab[0][i] = ram_store;
        mem_read_tab[1][i] = bios_read;
        mem_write_tab[1][i] = ram_store;
        mem_read_tab[2][i] = lo_read;
        mem_write_tab[2][i] = lo_store;
        mem_read_tab[3][i] = lo_read;
        mem_write_tab[3][i] = lo_store;
        mem_read_tab[4][i] = ram_read;
        mem_write_tab[4][i] = ram_store;
        mem_read_tab[5][i] = ram_read;
        mem_write_tab[5][i] = ram_store;
        mem_read_tab[6][i] = lo_read;
        mem_write_tab[6][i] = lo_store;
        mem_read_tab[7][i] = lo_read;
        mem_write_tab[7][i] = lo_store;
    }

    for (i = 0x10; i <= 0x13; i++) {
        mem_read_tab[0][i] = ram_read;
        mem_write_tab[0][i] = ram_store;
        mem_read_tab[1][i] = colorram_read;
        mem_write_tab[1][i] = colorram_store;
        mem_read_tab[2][i] = lo_read;
        mem_write_tab[2][i] = lo_store;
        mem_read_tab[3][i] = colorram_read;
        mem_write_tab[3][i] = colorram_store;
        mem_read_tab[4][i] = ram_read;
        mem_write_tab[4][i] = ram_store;
        mem_read_tab[5][i] = colorram_read;
        mem_write_tab[5][i] = colorram_store;
        mem_read_tab[6][i] = lo_read;
        mem_write_tab[6][i] = lo_store;
        mem_read_tab[7][i] = colorram_read;
        mem_write_tab[7][i] = colorram_store;
    }

    for (i = 0x14; i <= 0x3f; i++) {
        mem_read_tab[0][i] = ram_read;
        mem_write_tab[0][i] = ram_store;
        mem_read_tab[1][i] = ram_read;
        mem_write_tab[1][i] = ram_store;
        mem_read_tab[2][i] = lo_read;
        mem_write_tab[2][i] = lo_store;
        mem_read_tab[3][i] = lo_read;
        mem_write_tab[3][i] = lo_store;
        mem_read_tab[4][i] = ram_read;
        mem_write_tab[4][i] = ram_store;
        mem_read_tab[5][i] = ram_read;
        mem_write_tab[5][i] = ram_store;
        mem_read_tab[6][i] = lo_read;
        mem_write_tab[6][i] = lo_store;
        mem_read_tab[7][i] = lo_read;
        mem_write_tab[7][i] = lo_store;
    }

    for (j = 0; j < NUM_CONFIGS; j++) {
        for (i = 0x40; i <= 0xbf; i++) {
            mem_read_tab[j][i] = ram_read;
            mem_write_tab[j][i] = ram_store;
        }
    }

    for (i = 0xc0; i <= 0xcf; i++) {
        mem_read_tab[0][i] = ram_read;
        mem_write_tab[0][i] = ram_store;
        mem_read_tab[1][i] = ram_read;
        mem_write_tab[1][i] = ram_store;
        mem_read_tab[2][i] = top_shared_read;
        mem_write_tab[2][i] = top_shared_store;
        mem_read_tab[3][i] = top_shared_read;
        mem_write_tab[3][i] = top_shared_store;
        mem_read_tab[4][i] = ram_read;
        mem_write_tab[4][i] = ram_store;
        mem_read_tab[5][i] = ram_read;
        mem_write_tab[5][i] = ram_store;
        mem_read_tab[6][i] = top_shared_read;
        mem_write_tab[6][i] = top_shared_store;
        mem_read_tab[7][i] = top_shared_read;
        mem_write_tab[7][i] = top_shared_store;
    }

    for (i = 0xd0; i <= 0xdf; i++) {
        mem_read_tab[0][i] = ram_read;
        mem_write_tab[0][i] = ram_store;
        mem_read_tab[1][i] = ram_read;
        mem_write_tab[1][i] = ram_store;
        mem_read_tab[2][i] = top_shared_read;
        mem_write_tab[2][i] = top_shared_store;
        mem_read_tab[3][i] = top_shared_read;
        mem_write_tab[3][i] = top_shared_store;
        mem_read_tab[4][i] = ram_read;
        mem_write_tab[4][i] = ram_store;
        mem_read_tab[5][i] = ram_read;
        mem_write_tab[5][i] = ram_store;
        mem_read_tab[6][i] = top_shared_read;
        mem_write_tab[6][i] = top_shared_store;
        mem_read_tab[7][i] = top_shared_read;
        mem_write_tab[7][i] = top_shared_store;
    }

    for (i = 0xe0; i <= 0xfe; i++) {
        mem_read_tab[0][i] = ram_read;
        mem_write_tab[0][i] = ram_store;
        mem_read_tab[1][i] = ram_read;
        mem_write_tab[1][i] = ram_store;
        mem_read_tab[2][i] = top_shared_read;
        mem_write_tab[2][i] = top_shared_store;
        mem_read_tab[3][i] = top_shared_read;
        mem_write_tab[3][i] = top_shared_store;
        mem_read_tab[4][i] = ram_read;
        mem_write_tab[4][i] = ram_store;
        mem_read_tab[5][i] = ram_read;
        mem_write_tab[5][i] = ram_store;
        mem_read_tab[6][i] = top_shared_read;
        mem_write_tab[6][i] = top_shared_store;
        mem_read_tab[7][i] = top_shared_read;
        mem_write_tab[7][i] = top_shared_store;
    }

    for (j = 0; j < NUM_CONFIGS; j++) {
        mem_read_tab[j][0xff] = mmu_ffxx_read_z80;
        mem_write_tab[j][0xff] = mmu_ffxx_store;

        mem_read_tab[j][0x100] = mem_read_tab[j][0x0];
        mem_write_tab[j][0x100] = mem_write_tab[j][0x0];
    }

    _z80mem_read_tab_ptr = mem_read_tab[0];
    _z80mem_write_tab_ptr = mem_write_tab[0];
    _z80mem_read_base_tab_ptr = mem_read_base_tab[0];
    z80mem_read_limit_tab_ptr = mem_read_limit_tab[0];

    /* IO address space.  */

    /* At least we know what happens.  */
    for (i = 0; i <= 0x100; i++) {
        io_read_tab[i] = read_unconnected_io;
        io_write_tab[i] = store_unconnected_io;
    }
/*
    io_read_tab[0x10] = colorram_read;
    io_write_tab[0x10] = colorram_store;
    io_read_tab[0x11] = colorram_read;
    io_write_tab[0x11] = colorram_store;
    io_read_tab[0x12] = colorram_read;
    io_write_tab[0x12] = colorram_store;
    io_read_tab[0x13] = colorram_read;
    io_write_tab[0x13] = colorram_store;
*/
    io_read_tab[0xd0] = vicii_read;
    io_write_tab[0xd0] = vicii_store;
    io_read_tab[0xd1] = vicii_read;
    io_write_tab[0xd1] = vicii_store;
    io_read_tab[0xd2] = vicii_read;
    io_write_tab[0xd2] = vicii_store;
    io_read_tab[0xd3] = vicii_read;
    io_write_tab[0xd3] = vicii_store;

    io_read_tab[0xd4] = sid_read;
    io_write_tab[0xd4] = sid_store;

    io_read_tab[0xd5] = mmu_read;
    io_write_tab[0xd5] = mmu_store;

    io_read_tab[0xd6] = vdc_read;
    io_write_tab[0xd6] = vdc_store;
    io_read_tab[0xd7] = d7xx_read;
    io_write_tab[0xd7] = d7xx_store;

    io_read_tab[0xd8] = colorram_read;
    io_write_tab[0xd8] = colorram_store;
    io_read_tab[0xd9] = colorram_read;
    io_write_tab[0xd9] = colorram_store;
    io_read_tab[0xda] = colorram_read;
    io_write_tab[0xda] = colorram_store;
    io_read_tab[0xdb] = colorram_read;
    io_write_tab[0xdb] = colorram_store;

    io_read_tab[0xdc] = cia1_read;
    io_write_tab[0xdc] = cia1_store;
    io_read_tab[0xdd] = cia2_read;
    io_write_tab[0xdd] = cia2_store;

    io_read_tab[0xde] = c64io1_read;
    io_write_tab[0xde] = c64io1_store;
    io_read_tab[0xdf] = c64io2_read;
    io_write_tab[0xdf] = c64io2_store;
#endif
}

#ifdef _MSC_VER
#pragma optimize("",on)
#endif

void z80mem_set_bank_pointer(BYTE **base, int *limit)
{
    bank_base = base;
    bank_limit = limit;
}

void z80mem_update_config(int config)
{
    _z80mem_read_tab_ptr = mem_read_tab[config];
    _z80mem_write_tab_ptr = mem_write_tab[config];
    _z80mem_read_base_tab_ptr = mem_read_base_tab[config];
    z80mem_read_limit_tab_ptr = mem_read_limit_tab[config];
/*
    if (bank_limit != NULL) {
        *bank_base = _z80mem_read_base_tab_ptr[z80_old_reg_pc >> 8];
        if (*bank_base != 0)
            *bank_base = _z80mem_read_base_tab_ptr[z80_old_reg_pc >> 8]
                         - (z80_old_reg_pc & 0xff00);
        *bank_limit = z80mem_read_limit_tab_ptr[z80_old_reg_pc >> 8];
    }
*/
}

int z80mem_load(void)
{
//  if (z80mem_log == LOG_ERR)			// [AppleWin-TC]
//      z80mem_log = log_open("Z80MEM");

    z80mem_initialize();

    return 0;
}

