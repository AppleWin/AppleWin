#pragma once
#include "Card.h"
#include "CopyProtectionDongles.h"

#define  REGLOAD(a, b)				RegLoadValue(TEXT(REG_CONFIG), (a), TRUE, (b))
#define  REGLOAD_DEFAULT(a, b, c)	RegLoadValue(TEXT(REG_CONFIG), (a), TRUE, (b), (c))
#define  REGSAVE(a, b)				RegSaveValue(TEXT(REG_CONFIG), (a), TRUE, (b))

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, uint32_t chars);
BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, uint32_t chars, LPCTSTR defaultValue);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, uint32_t* value);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, uint32_t* value, uint32_t defaultValue);
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer);
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, uint32_t value);

std::string RegGetConfigSlotSection(UINT slot);
void RegDeleteConfigSlotSection(UINT slot);
void RegSetConfigSlotNewCardType(UINT slot, SS_CARDTYPE type);
void RegSetConfigGameIOConnectorNewDongleType(UINT slot, DONGLETYPE type);
