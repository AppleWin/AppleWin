#pragma once

#include "Disk.h"
#include "Harddisk.h"


void LoadConfiguration();
void InsertFloppyDisks(const UINT slot, LPSTR szImageName_drive[NUM_DRIVES], bool driveConnected[NUM_DRIVES], bool& bBoot);
void InsertHardDisks(LPSTR szImageName_harddisk[NUM_HARDDISKS], bool& bBoot);
void UnplugHardDiskControllerCard(void);
void GetAppleWindowTitle();

void CtrlReset();
void ResetMachineState();
