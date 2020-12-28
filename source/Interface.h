#pragma once

// an AppleWin frontend must provide the implementation of these methods
//
// once this is done,
// the core emulator files (i.e. almost all the .cpp directly in Source)
// can compile, link and run properly
// this does not include the main event loop which is left in the arch specific area
// nor the actual rendering of the video buffer to screen

#include "Video.h"
Video& GetVideo(void);

#include "Configuration/IPropertySheet.h"
IPropertySheet& GetPropertySheet(void);

#include "FrameBase.h"
FrameBase& GetFrame(void);
