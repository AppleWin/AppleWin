#pragma once
#include "Card.h"

#define  REGLOAD(a, b)				RegLoadValue(TEXT(REG_CONFIG), (a), TRUE, (b))
#define  REGLOAD_DEFAULT(a, b, c)	RegLoadValue(TEXT(REG_CONFIG), (a), TRUE, (b), (c))
#define  REGSAVE(a, b)				RegSaveValue(TEXT(REG_CONFIG), (a), TRUE, (b))

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars);
BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars, LPCTSTR defaultValue);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD* value);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD* value, DWORD defaultValue);
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer);
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value);

std::string RegGetConfigSlotSection(UINT slot);
void RegDeleteConfigSlotSection(UINT slot);
void RegSetConfigSlotNewCardType(UINT slot, enum SS_CARDTYPE type);
