#include "StdAfx.h"

#include "FrameBase.h"
#include "Interface.h"

FrameBase::FrameBase()
{
	g_hFrameWindow = (HWND)0;
	g_bConfirmReboot = 1; // saved PageConfig REGSAVE
	g_bMultiMon = 0; // OFF = load window position & clamp initial frame to screen, ON = use window position as is
	g_bFreshReset = false;
	g_hInstance = (HINSTANCE)0;
}

FrameBase::~FrameBase()
{

}

void FrameBase::VideoRedrawScreen(void)
{
	// NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
	GetVideo().VideoRefreshScreen(GetVideo().GetVideoMode(), true);
}
