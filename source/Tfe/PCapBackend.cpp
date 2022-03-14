/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2022, Andrea Odetti

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

#include "StdAfx.h"

#include "PCapBackend.h"
#include "tfearch.h"
#include "../Common.h"
#include "../Registry.h"

std::string PCapBackend::tfe_interface;

PCapBackend::PCapBackend(const std::string & pcapInterface)
{
	tfePcapFP = TfePcapOpenAdapter(pcapInterface);
}

PCapBackend::~PCapBackend()
{
	TfePcapCloseAdapter(tfePcapFP);
	tfePcapFP = NULL;
}

void PCapBackend::transmit(
    const int txlength,	    /* Frame length */
    uint8_t *txframe        /* Pointer to the frame to be transmitted */
)
{
    if (tfePcapFP)
    {
        tfe_arch_transmit(tfePcapFP, txlength, txframe);
    }
}

int PCapBackend::receive(const int size, uint8_t * rxframe)
{
    if (tfePcapFP)
    {
        return tfe_arch_receive(tfePcapFP, size, rxframe);
    }
    else
    {
        return -1;
    }
}

bool PCapBackend::isValid()
{
    return tfePcapFP;
}

void PCapBackend::update(const ULONG /* nExecutedCycles */)
{
    // nothing to do
}

int PCapBackend::tfe_enumadapter_open(void)
{
    return tfe_arch_enumadapter_open();
}

int PCapBackend::tfe_enumadapter(std::string & name, std::string & description)
{
    return tfe_arch_enumadapter(name, description);
}

int PCapBackend::tfe_enumadapter_close(void)
{
    return tfe_arch_enumadapter_close();
}

void PCapBackend::tfe_SetRegistryInterface(UINT slot, const std::string& name)
{
    std::string regSection = RegGetConfigSlotSection(slot);
    RegSaveString(regSection.c_str(), REGVALUE_UTHERNET_INTERFACE, 1, name);
}

void PCapBackend::get_disabled_state(int * param)
{
    *param = tfe_cannot_use;
}
