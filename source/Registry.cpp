/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Registry module
 *
 * Author: Various
 */

#include "StdAfx.h"


//===========================================================================
BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser,
                    LPTSTR buffer, DWORD chars) {
  int  success = 0;
  TCHAR fullkeyname[256];
  wsprintf(fullkeyname,
           TEXT("Software\\AppleWin\\CurrentVersion\\%s"),
           (LPCTSTR)section);
  HKEY keyhandle;
  if (!RegOpenKeyEx((peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
                    fullkeyname,
                    0,
                    KEY_READ,
                    &keyhandle)) {
    DWORD type;
    DWORD size = chars;
    success = (!RegQueryValueEx(keyhandle,key,0,&type,(LPBYTE)buffer,&size)) &&
                                size;
    RegCloseKey(keyhandle);
  }
  return success;
}

//===========================================================================
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value) {
  if (!value)
    return 0;
  TCHAR buffer[32] = TEXT("");
  if (!RegLoadString(section,key,peruser,buffer,32))
    return 0;
  buffer[31] = 0;
  *value = (DWORD)_ttoi(buffer);
  return 1;
}

//===========================================================================
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPCTSTR buffer) {
  TCHAR fullkeyname[256];
  wsprintf(fullkeyname,
           TEXT("Software\\AppleWin\\CurrentVersion\\%s"),
           (LPCTSTR)section);
  HKEY  keyhandle;
  DWORD disposition;
  if (!RegCreateKeyEx((peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
                      fullkeyname,
                      0,
                      NULL,
                      REG_OPTION_NON_VOLATILE,
                      KEY_READ | KEY_WRITE,
                      (LPSECURITY_ATTRIBUTES)NULL,
                      &keyhandle,
                      &disposition)) {
    RegSetValueEx(keyhandle,
                  key,
                  0,
                  REG_SZ,
                  (CONST BYTE *)buffer,
                  (_tcslen(buffer)+1)*sizeof(TCHAR));
    RegCloseKey(keyhandle);
  }
}

//===========================================================================
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value) {
  TCHAR buffer[32] = TEXT("");
  _ultot(value,buffer,10);
  RegSaveString(section,key,peruser,buffer);
}
