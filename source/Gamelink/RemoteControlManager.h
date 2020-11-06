#pragma once

#include "Gamelink/Gamelink.h"

class RemoteControlManager
{
public:
	RemoteControlManager(void) {}
	~RemoteControlManager(void) {}

	LPBYTE RemoteControlManager::initializeMem(UINT size);
	bool destroyMem();
	void RemoteControlManager::setRunningProgramInfo();
	void setWozSignature(std::string Title, std::string Subtitle, std::string Version, UINT32 iCrc32);
	void setHdvSignature(std::string VolumeName);
	void getInput();
	void sendOutput(LPBITMAPINFO g_pFramebufferinfo, UINT8* g_pFramebufferbits);

	static bool RemoteControlManager::isRemoteControlEnabled();

	UINT const kMinRepeatInterval = 400;	// Minimum keypress repeat message interval in ms
};
