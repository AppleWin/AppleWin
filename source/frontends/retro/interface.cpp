#include "linux/interface.h"

#include "frontends/retro/environment.h"
#include "linux/win.h"

int MessageBox(HWND, const char * text, const char * caption, UINT type)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s: %s - %s\n", __FUNCTION__, caption, text);
  return IDOK;
}

void FrameDrawDiskLEDS(HDC x)
{
}

void FrameDrawDiskStatus(HDC x)
{
}

void FrameRefreshStatus(int x, bool)
{
}
