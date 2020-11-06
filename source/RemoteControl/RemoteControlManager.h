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

	static bool RemoteControlManager::isRemoteControlEnabled();
	static void RemoteControlManager::setRemoteControlEnabled(bool bEnabled);
	static bool RemoteControlManager::isTrackOnlyEnabled();
	static void RemoteControlManager::setTrackOnlyEnabled(bool bEnabled);

	UINT const kMinRepeatInterval = 400;	// Minimum keypress repeat message interval in ms
};
