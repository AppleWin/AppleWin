#pragma once

#include "RgbMonitor.h"
#include "Harddisk.h"
#include "Disk.h"
#include "Common.h"
#include "Card.h"


struct CmdLine
{
	CmdLine()
	{
		bShutdown = false;
		bSetFullScreen = false;
		bBoot = false;
		bChangedDisplayResolution = false;
		bSlot0LanguageCard = false;
		bSlot7EmptyOnExit = false;
		bSwapButtons0and1 = false;
		bRemoveNoSlotClock = false;
		bestWidth = 0;
		bestHeight = 0;
		szImageName_harddisk[HARDDISK_1] = NULL;
		szImageName_harddisk[HARDDISK_2] = NULL;
		szSnapshotName = NULL;
		szScreenshotFilename = NULL;
		uRamWorksExPages = 0;
		uSaturnBanks = 0;
		newVideoType = -1;
		newVideoStyleEnableMask = 0;
		newVideoStyleDisableMask = 0;
		newVideoRefreshRate = VR_NONE;
		clockMultiplier = 0.0;	// 0 => not set from cmd-line
		model = A2TYPE_MAX;
		rgbCard = RGB_Videocard_e::Apple;
		rgbCardForegroundColor = 15;
		rgbCardBackgroundColor = 0;

		for (UINT i = 0; i < NUM_SLOTS; i++)
		{
			bSlotEmpty[i] = false;
			slotInsert[i] = CT_Empty;
			szImageName_drive[i][DRIVE_1] = NULL;
			szImageName_drive[i][DRIVE_2] = NULL;
			driveConnected[i][DRIVE_1] = true;
			driveConnected[i][DRIVE_2] = true;
		}
	}

	bool bShutdown;
	bool bSetFullScreen;
	bool bBoot;
	bool bChangedDisplayResolution;
	bool bSlot0LanguageCard;
	bool bSlotEmpty[NUM_SLOTS];
	bool bSlot7EmptyOnExit;
	bool bSwapButtons0and1;
	bool bRemoveNoSlotClock;
	SS_CARDTYPE slotInsert[NUM_SLOTS];
	UINT bestWidth;
	UINT bestHeight;
	LPSTR szImageName_drive[NUM_SLOTS][NUM_DRIVES];
	bool driveConnected[NUM_SLOTS][NUM_DRIVES];
	LPSTR szImageName_harddisk[NUM_HARDDISKS];
	LPSTR szSnapshotName;
	LPSTR szScreenshotFilename;
	UINT uRamWorksExPages;
	UINT uSaturnBanks;
	int newVideoType;
	int newVideoStyleEnableMask;
	int newVideoStyleDisableMask;
	VideoRefreshRate_e newVideoRefreshRate;
	double clockMultiplier;
	eApple2Type model;
	RGB_Videocard_e rgbCard;
	int rgbCardForegroundColor;
	int rgbCardBackgroundColor;
	std::string strCurrentDir;
};

bool ProcessCmdLine(LPSTR lpCmdLine);

extern std::string g_sConfigFile; // INI file to use instead of Registry
extern bool g_bCapturePrintScreenKey;
extern bool g_bRegisterFileTypes;
extern bool g_bHookSystemKey;
extern bool g_bHookAltTab;
extern bool g_bHookAltGrControl;
extern CmdLine g_cmdLine;
