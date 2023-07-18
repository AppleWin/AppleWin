#pragma once

#include "Common.h"

// Must be in the same order as in PageAdvanced.cpp
enum DONGLETYPE { DT_EMPTY, DT_SDSSPEEDSTAR, DT_CODEWRITER, DT_ROBOCOM500, DT_ROBOCOM1000, DT_ROBOCOM1500 };

void SetCopyProtectionDongleType(DONGLETYPE type);
DONGLETYPE GetCopyProtectionDongleType(void);
void DongleControl(WORD address);
int CopyProtectionDonglePB0(void);
int CopyProtectionDonglePB1(void);
int CopyProtectionDonglePB2(void);
int CopyProtectionDonglePDL(UINT pdl);

void CopyProtectionDongleSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void CopyProtectionDongleLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);
