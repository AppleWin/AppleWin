#pragma once

#include "DiskImage.h"

class RemoteControlManager
{
public:
	RemoteControlManager(void) {}
	~RemoteControlManager(void) {}

	LPBYTE RemoteControlManager::initializeMem(UINT size);
	bool destroyMem();
	void RemoteControlManager::setLoadedFloppyInfo(ImageInfo* imageInfo);
	void RemoteControlManager::setLoadedHDInfo(ImageInfo* imageInfo);
	void getInput();
	void sendOutput(LPBITMAPINFO g_pFramebufferinfo, UINT8* g_pFramebufferbits);

	static bool RemoteControlManager::isRemoteControlEnabled();
	static void RemoteControlManager::setRemoteControlEnabled(bool bEnabled);

	UINT const kMinRepeatInterval = 400;	// Minimum keypress repeat message interval in ms
};
