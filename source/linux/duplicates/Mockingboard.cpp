#include "StdAfx.h"

#include "Common.h"

void    MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5)
{
}

void    MB_UpdateCycles(ULONG uExecutedCycles)
{
}

void    MB_StartOfCpuExecute()
{
}

void MB_SetCumulativeCycles()
{
}

void MB_SaveSnapshot(YamlSaveHelper&, unsigned int)
{
}

bool MB_LoadSnapshot(class YamlLoadHelper&, UINT, UINT)
{
  return true;
}

void MB_Reset()
{
}

void MB_InitializeForLoadingSnapshot()
{
}

bool Phasor_LoadSnapshot(class YamlLoadHelper&, UINT, UINT)
{
  return true;
}

void Phasor_SaveSnapshot(YamlSaveHelper&, unsigned int)
{
}

std::string MB_GetSnapshotCardName()
{
  return "Mockingboard C";
}

std::string Phasor_GetSnapshotCardName()
{
  return "Phasor";
}
