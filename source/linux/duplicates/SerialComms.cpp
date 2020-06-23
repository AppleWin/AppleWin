#include "StdAfx.h"

#include "Common.h"
#include "SerialComms.h"


CSuperSerialCard::CSuperSerialCard(UINT slot) :
  Card(CT_SSC), m_uSlot(slot)
{
}

CSuperSerialCard::~CSuperSerialCard()
{
}

void CSuperSerialCard::CommReset()
{
}

void CSuperSerialCard::CommInitialize(LPBYTE pCxRomPeripheral, UINT uSlot)
{
}
