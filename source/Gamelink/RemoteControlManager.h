#pragma once

#include "Gamelink/Gamelink.h"

class RemoteControlManager
{
public:
	RemoteControlManager(void) {}
	~RemoteControlManager(void) {}

	void initialize();
	void RemoteControlManager::getInput();
	void RemoteControlManager::sendOutput(LPBITMAPINFO g_pFramebufferinfo, UINT8* g_pFramebufferbits);

	UINT const kMinRepeatInterval = 400;	// Minimum keypress repeat message interval in ms
};
