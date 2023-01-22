#pragma once

#include "Common.h"

// Must be in the same order as in PageAdvanced.cpp
enum DONGLETYPE { DT_EMPTY, DT_SDSSPEEDSTAR };

void SetCopyProtectionDongleType(DONGLETYPE type);
DONGLETYPE GetCopyProtectionDongleType(void);
int CopyProtectionDonglePB0(void);
int CopyProtectionDonglePB1(void);
int CopyProtectionDonglePB2(void);
bool SdsSpeedStar(void);

void CopyProtectionDongleSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void CopyProtectionDongleLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);
