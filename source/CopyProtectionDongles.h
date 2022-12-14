#pragma once

#include "Common.h"


void SetCpyPrtDongleType(DWORD type);
DWORD GetCpyPrtDongleType(void);
void CopyProtDongleControl(const UINT uControl);
DWORD CopyProtDonglePB0(void);
DWORD CopyProtDonglePB1(void);
DWORD CopyProtDonglePB2(void);
bool SdsSpeedStar(void);
