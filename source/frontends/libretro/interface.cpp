#include "linux/interface.h"

#include "frontends/libretro/environment.h"
#include "linux/win.h"

int MessageBox(HWND, const char * text, const char * caption, UINT type)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s: %s - %s\n", __FUNCTION__, caption, text);
  return IDOK;
}
