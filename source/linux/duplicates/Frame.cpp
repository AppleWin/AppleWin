#include "StdAfx.h"

#include "Common.h"

void FrameSetCursorPosByMousePos()
{
}

UINT GetFrameBufferBorderlessWidth(void)
{
	static const UINT uFrameBufferBorderlessW = 560;	// 560 = Double Hi-Res
	return uFrameBufferBorderlessW;
}

UINT GetFrameBufferBorderlessHeight(void)
{
	static const UINT uFrameBufferBorderlessH = 384;	// 384 = Double Scan Line
	return uFrameBufferBorderlessH;
}

// NB. These border areas are not visible (... and these border areas are unrelated to the 3D border below)
UINT GetFrameBufferBorderWidth(void)
{
	static const UINT uBorderW = 20;
	return uBorderW;
}

UINT GetFrameBufferBorderHeight(void)
{
	static const UINT uBorderH = 18;
	return uBorderH;
}

UINT GetFrameBufferWidth(void)
{
	return GetFrameBufferBorderlessWidth() + 2*GetFrameBufferBorderWidth();
}

UINT GetFrameBufferHeight(void)
{
	return GetFrameBufferBorderlessHeight() + 2*GetFrameBufferBorderHeight();
}

void FrameUpdateApple2Type()
{
}
