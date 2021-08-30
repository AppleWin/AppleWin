#pragma once

//#include "Card.h"

const UINT JOYSTICKSTATIONARY = 0x20;

void Configure4Play(LPBYTE pCxRomPeripheral, UINT uSlot);

std::string FourPlay_GetSnapshotCardName(void);
void FourPlay_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const UINT uSlot);
bool FourPlay_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);
