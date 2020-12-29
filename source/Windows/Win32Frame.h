#pragma once

#include "FrameBase.h"

class Win32Frame : public FrameBase
{
public:
	virtual void FrameDrawDiskLEDS(HDC hdc);
	virtual void FrameDrawDiskStatus(HDC hdc);
	virtual void FrameRefreshStatus(int, bool bUpdateDiskStatus = true);
	virtual void FrameUpdateApple2Type();
	virtual void FrameSetCursorPosByMousePos();

	virtual void SetFullScreenShowSubunitStatus(bool bShow);
	virtual bool GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight = 0);
	virtual int SetViewportScale(int nNewScale, bool bForce = false);
	virtual void SetAltEnterToggleFullScreen(bool mode);

	virtual void SetLoadedSaveStateFlag(const bool bFlag);
};
