#pragma once

#define  REGLOAD(a,b) RegLoadValue(TEXT(REG_CONFIG),a,1,b)
#define  REGSAVE(a,b) RegSaveValue(TEXT(REG_CONFIG),a,1,b)

BOOL    RegLoadString (LPCTSTR,LPCTSTR,BOOL,LPTSTR,DWORD);
BOOL    RegLoadValue (LPCTSTR,LPCTSTR,BOOL,DWORD *);
void    RegSaveString (LPCTSTR,LPCTSTR,BOOL,LPCTSTR);
void    RegSaveValue (LPCTSTR,LPCTSTR,BOOL,DWORD);

BOOL    RegLoadValue (LPCTSTR,LPCTSTR,BOOL,BOOL *);
