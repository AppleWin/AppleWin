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

#ifdef _MSC_VER
#include <iphlpapi.h>
#endif

PCapBackend::PCapBackend(const std::string & interfaceName) : m_interfaceName(interfaceName)
{
	m_tfePcapFP = TfePcapOpenAdapter(interfaceName);
}

PCapBackend::~PCapBackend()
{
	TfePcapCloseAdapter(m_tfePcapFP);
	m_tfePcapFP = NULL;
}

void PCapBackend::transmit(
    const int txlength,	    /* Frame length */
    uint8_t *txframe        /* Pointer to the frame to be transmitted */
)
{
    if (m_tfePcapFP)
    {
        tfe_arch_transmit(m_tfePcapFP, txlength, txframe);
    }
}

int PCapBackend::receive(const int size, uint8_t * rxframe)
{
    if (m_tfePcapFP)
    {
        return tfe_arch_receive(m_tfePcapFP, size, rxframe);
    }
    else
    {
        return -1;
    }
}

bool PCapBackend::isValid()
{
    return m_tfePcapFP;
}

void PCapBackend::update(const ULONG /* nExecutedCycles */)
{
    // nothing to do
}

void PCapBackend::getMACAddress(const uint32_t address, MACAddress & mac)
{
    // this is only expected to be called for IP addresses on the same network
#ifdef _MSC_VER
    const DWORD dwSourceAddress = INADDR_ANY;
    ULONG len = sizeof(MACAddress::address);
    SendARP(address, dwSourceAddress, mac.address, &len);
#endif
}

const std::string & PCapBackend::getInterfaceName()
{
    return m_interfaceName;
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

const char * PCapBackend::tfe_lib_version(void)
{
    return tfe_arch_lib_version();
}

void PCapBackend::SetRegistryInterface(UINT slot, const std::string& name)
{
    std::string regSection = RegGetConfigSlotSection(slot);
    RegSaveString(regSection.c_str(), REGVALUE_UTHERNET_INTERFACE, 1, name);
}

std::string PCapBackend::GetRegistryInterface(UINT slot)
{
    char interfaceName[MAX_PATH];
    std::string regSection = RegGetConfigSlotSection(slot);
    RegLoadString(regSection.c_str(), REGVALUE_UTHERNET_INTERFACE, TRUE, interfaceName, sizeof(interfaceName), TEXT(""));
    return interfaceName;
}

int PCapBackend::tfe_is_npcap_loaded()
{
    return tfe_arch_is_npcap_loaded();
}
