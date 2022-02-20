/*
 * tfe.c - TFE ("The final ethernet") emulation.
 *
 * Written by
 *  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>
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

#include "../StdAfx.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tfe.h"
#include "tfearch.h"
#include "tfesupp.h"

#ifndef NULL
#define NULL 0
#endif
typedef unsigned int UINT;

#include "../Common.h"	// For: IS_APPLE2
#include "../Registry.h"


/* Flag: Do we use the "original" memory map or the memory map of the RR-Net? */
//static int tfe_as_rr_net = 0;

/* ------------------------------------------------------------------------- */
/*    functions for selecting and querying available NICs                    */


// Called by: tfe_LoadSnapshot() & ApplyNewConfig()
void tfe_SetRegistryInterface(UINT slot, const std::string& name)
{
    std::string regSection = RegGetConfigSlotSection(slot);
    RegSaveString(regSection.c_str(), REGVALUE_UTHERNET_INTERFACE, 1, name);
}

/* ------------------------------------------------------------------------- */
/*    functions for selecting and querying available NICs                    */

int tfe_enumadapter_open(void)
{
    return tfe_arch_enumadapter_open();
}

int tfe_enumadapter(char **ppname, char **ppdescription)
{
    return tfe_arch_enumadapter(ppname, ppdescription);
}

int tfe_enumadapter_close(void)
{
    return tfe_arch_enumadapter_close();
}

void get_disabled_state(int * param)
{
    *param = tfe_cannot_use;
}

//#endif /* #ifdef HAVE_TFE */
