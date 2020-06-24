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

bool CSuperSerialCard::LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int)
{
  return true;
}

void CSuperSerialCard::SaveSnapshot(YamlSaveHelper&)
{
}

std::string CSuperSerialCard::GetSnapshotCardName()
{
  return "Super Serial Card";
}
