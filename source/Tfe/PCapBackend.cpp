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
    int txlength,			/* Frame length */
    uint8_t *txframe        /* Pointer to the frame to be transmitted */
)
{
    if (tfePcapFP)
    {
        tfe_arch_transmit(tfePcapFP, txlength, txframe);
    }
}

int PCapBackend::receive(uint8_t * data, int * size)
{
    if (tfePcapFP)
    {
        return tfe_arch_receive(tfePcapFP, data, size);
    }
    else
    {
        return 0;
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

int PCapBackend::tfe_enumadapter(char **ppname, char **ppdescription)
{
    return tfe_arch_enumadapter(ppname, ppdescription);
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
