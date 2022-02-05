#include "StdAfx.h"

#include "PCapBackend.h"
#include "tfearch.h"

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
}
