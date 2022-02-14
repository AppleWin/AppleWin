#include "StdAfx.h"

#include "FrameBase.h"
#include "Interface.h"
#include "Core.h"
#include "NTSC.h"

FrameBase::FrameBase()
{
	g_hFrameWindow = (HWND)0;
	g_bConfirmReboot = 1; // saved PageConfig REGSAVE
	g_bMultiMon = 0; // OFF = load window position & clamp initial frame to screen, ON = use window position as is
	g_bFreshReset = false;
	g_hInstance = (HINSTANCE)0;
	g_bDisplayPrintScreenFileName = false;
	g_bShowPrintScreenWarningDialog = true;
}

FrameBase::~FrameBase()
{

}

void FrameBase::VideoRefreshScreen(uint32_t uRedrawWholeScreenVideoMode, bool bRedrawWholeScreen)
{
	// regenerate video buffer if needed
	GetVideo().VideoRefreshBuffer(uRedrawWholeScreenVideoMode, bRedrawWholeScreen);

	// actually draw it on screen
	VideoPresentScreen();
}

void FrameBase::VideoRedrawScreen(void)
{
	// NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
	VideoRefreshScreen(GetVideo().GetVideoMode(), true);
}

//===========================================================================
void FrameBase::VideoRedrawScreenDuringFullSpeed(DWORD dwCyclesThisFrame, bool bInit /*=false*/)
{
	if (bInit)
	{
		// Just entered full-speed mode
		dwFullSpeedStartTime = GetTickCount();
		return;
	}

	DWORD dwFullSpeedDuration = GetTickCount() - dwFullSpeedStartTime;
	if (dwFullSpeedDuration <= 16)	// Only update after every realtime ~17ms of *continuous* full-speed
		return;

	dwFullSpeedStartTime += dwFullSpeedDuration;

	VideoRedrawScreenAfterFullSpeed(dwCyclesThisFrame);
}

void FrameBase::VideoRedrawScreenAfterFullSpeed(DWORD dwCyclesThisFrame)
{
	NTSC_VideoClockResync(dwCyclesThisFrame);
	VideoRedrawScreen();	// Better (no flicker) than using: NTSC_VideoReinitialize() or VideoReinitialize()
}

void FrameBase::Video_RedrawAndTakeScreenShot(const char* pScreenshotFilename)
{
	_ASSERT(pScreenshotFilename);
	if (!pScreenshotFilename)
		return;

	VideoRedrawScreen();
	Video_SaveScreenShot(Video::SCREENSHOT_560x384, pScreenshotFilename);
}

void FrameBase::Video_TakeScreenShot(const Video::VideoScreenShot_e ScreenShotType)
{
	std::string strScreenShotFileName;

	// find last screenshot filename so we don't overwrite the existing user ones
	bool bExists = true;
	while (bExists)
	{
		if (g_nLastScreenShot > nMaxScreenShot) // Holy Crap! User has maxed the number of screenshots!?
		{
			std::string msg = StrFormat("You have more then %d screenshot filenames!  They will no longer be saved.\n\n"
										"Either move some of your screenshots or increase the maximum in video.cpp\n",
										nMaxScreenShot);
			FrameMessageBox(msg.c_str(), "Warning", MB_OK);
			g_nLastScreenShot = 0;
			return;
		}

		strScreenShotFileName = Util_MakeScreenShotFileName();
		bExists = Util_TestScreenShotFileName(strScreenShotFileName.c_str());
		if (!bExists)
		{
			break;
		}
		g_nLastScreenShot++;
	}

	Video_SaveScreenShot(ScreenShotType, strScreenShotFileName.c_str());
	g_nLastScreenShot++;
}

//===========================================================================

void FrameBase::Video_SaveScreenShot(const Video::VideoScreenShot_e ScreenShotType, const TCHAR* pScreenShotFileName)
{
	FILE* pFile = fopen(pScreenShotFileName, "wb");
	if (pFile)
	{
		GetVideo().Video_MakeScreenShot(pFile, ScreenShotType);
		fclose(pFile);
	}

	if (g_bDisplayPrintScreenFileName)
	{
		FrameMessageBox(pScreenShotFileName, "Screen Captured", MB_OK);
	}
}

std::string FrameBase::Util_MakeScreenShotFileName() const
{
	const std::string sPrefixScreenShotFileName = "AppleWin_ScreenShot";
	const std::string pPrefixFileName = !g_pLastDiskImageName.empty() ? g_pLastDiskImageName : sPrefixScreenShotFileName;
	const std::string folder = Video_GetScreenShotFolder();
	return StrFormat("%s%s_%09d.bmp", folder.c_str(), pPrefixFileName.c_str(), g_nLastScreenShot);
}

// Returns TRUE if file exists, else FALSE
bool FrameBase::Util_TestScreenShotFileName(const TCHAR* pFileName)
{
	bool bFileExists = false;
	FILE* pFile = fopen(pFileName, "rt");
	if (pFile)
	{
		fclose(pFile);
		bFileExists = true;
	}
	return bFileExists;
}

void FrameBase::Video_ResetScreenshotCounter(const std::string& pImageName)
{
	g_nLastScreenShot = 0;
	g_pLastDiskImageName = pImageName;
}
