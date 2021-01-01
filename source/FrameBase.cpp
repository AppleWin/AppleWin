#include "StdAfx.h"

#include "FrameBase.h"

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
