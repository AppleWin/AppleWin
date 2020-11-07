#pragma once

#include "DiskImage.h"

class RemoteControlManager
{
public:
	RemoteControlManager(void) {}
	~RemoteControlManager(void) {}

	LPBYTE initializeMem(UINT size);
	bool destroyMem();
	void setLoadedFloppyInfo(ImageInfo* imageInfo);
	void setLoadedHDInfo(ImageInfo* imageInfo);
	void getInput();
	void sendOutput(LPBITMAPINFO g_pFramebufferinfo, UINT8* g_pFramebufferbits);
	void updateRunningProgramInfo();
	void setKeypressExclusionList(UINT8 exclusionList[], UINT8 length);

	static bool RemoteControlManager::isRemoteControlEnabled();
	static void RemoteControlManager::setRemoteControlEnabled(bool bEnabled);
	static bool RemoteControlManager::isTrackOnlyEnabled();
	static void RemoteControlManager::setTrackOnlyEnabled(bool bEnabled);

	UINT const kMinRepeatInterval = 400;	// Minimum keypress repeat message interval in ms
};

static UINT8 aDefaultKeyExclusionList[] =
{
	VK_PRIOR,
	VK_NEXT,
	VK_END,
	VK_HOME,
	VK_SELECT,
	VK_PRINT,
	VK_EXECUTE,
	VK_SNAPSHOT,
	VK_INSERT,
	VK_DELETE,
	VK_HELP,
	VK_F1,
	VK_F2,
	VK_F3,
	VK_F4,
	VK_F5,
	VK_F6,
	VK_F7,
	VK_F8,
	// VK_F9,		// switch video modes
	// VK_F10,		// toggle rocker switch
	VK_F11,
	VK_F12,
	VK_F13,
	VK_F14,
	VK_F15,
	VK_F16,
	VK_F17,
	VK_F18,
	VK_F19,
	VK_F20,
	VK_F21,
	VK_F22,
	VK_F23,
	VK_F24,
	0x88,
	0x89,
	0x8A,
	0x8B,
	0x8C,
	0x8D,
	0x8E,
	0x8F,
	VK_NUMLOCK,
	// VK_SCROLL,		// toggle full-speed
	0xA6,
	0xA7,
	0xA8,
	0xA9,
	0xAA,
	0xAB,
	0xAC,
	0xAD,
	0xAE,
	0xAF,
	0xB0,
	0xB1,
	0xB2,
	0xB3,
	0xB4,
	0xB5,
	0xB6,
	0xB7,
	0xC3,
	0xC4,
	0xC5,
	0xC6,
	0xC7,
	0xC8,
	0xC9,
	0xCA,
	0xCB,
	0xCC,
	0xCD,
	0xCE,
	0xCF,
	0xD0,
	0xD1,
	0xD2,
	0xD3,
	0xD4,
	0xD5,
	0xD6,
	0xD7,
	0xD8,
	0xD9,
	0xDA,
	VK_ICO_HELP,
	VK_ICO_00,
	VK_PROCESSKEY,
	VK_ICO_CLEAR,
	0xE7,
	VK_OEM_RESET,
	VK_OEM_JUMP,
	VK_OEM_PA1,
	VK_OEM_PA2,
	VK_OEM_PA3,
	VK_OEM_WSCTRL,
	VK_OEM_CUSEL,
	VK_OEM_ATTN,
	VK_OEM_FINISH,
	VK_OEM_COPY,
	VK_OEM_AUTO,
	VK_OEM_ENLW,
	VK_OEM_BACKTAB,
	VK_ATTN,
	VK_CRSEL,
	VK_EXSEL,
	VK_EREOF,
	VK_PLAY,
	VK_ZOOM,
	VK_NONAME,
	VK_PA1,
	VK_OEM_CLEAR
};
