#pragma once
#include "Card.h"
#include "CopyProtectionDongles.h"
#include <string>
#include <type_traits>

bool RegLoadString (LPCTSTR section, LPCTSTR key, bool peruser, LPTSTR buffer, uint32_t chars);
bool RegLoadString (LPCTSTR section, LPCTSTR key, bool peruser, LPTSTR buffer, uint32_t chars, LPCTSTR defaultValue);
bool RegLoadValue (LPCTSTR section, LPCTSTR key, bool peruser, uint32_t* value);
bool RegLoadValue (LPCTSTR section, LPCTSTR key, bool peruser, uint32_t* value, uint32_t defaultValue);

inline bool REGLOAD(LPCTSTR a, uint32_t* b) {
    return RegLoadValue(REG_CONFIG, a, true, b);
}
inline bool REGLOAD_DEFAULT(LPCTSTR a, uint32_t* b, uint32_t c) {
    return RegLoadValue(REG_CONFIG, a, true, b, c);
}

void RegSaveString(LPCTSTR section, LPCTSTR key, bool peruser, const std::string & buffer);
template<typename V, typename std::enable_if_t<std::is_same_v<V, bool>, int> = 0>
void RegSaveValue(LPCTSTR section, LPCTSTR key, bool peruser, V value) {
    RegSaveString(section, key, peruser, value ? "1" : "0");
}
template<typename V, typename std::enable_if_t<std::is_signed_v<V> && !std::is_same_v<V, bool>, int> = 0>
void RegSaveValue(LPCTSTR section, LPCTSTR key, bool peruser, V value) {
    RegSaveString(section, key, peruser, std::to_string(value));
}
template<typename V, typename std::enable_if_t<std::is_unsigned_v<V> && !std::is_same_v<V, bool>, int> = 0>
void RegSaveValue(LPCTSTR section, LPCTSTR key, bool peruser, V value) {
    RegSaveValue<typename std::make_signed_t<V>>(section, key, peruser, value);
}
template<typename V, typename std::enable_if_t<std::is_enum_v<V>, int> = 0>
void RegSaveValue(LPCTSTR section, LPCTSTR key, bool peruser, V value) {
    RegSaveValue<typename std::underlying_type_t<V>>(section, key, peruser, value);
}

template<typename V>
inline void REGSAVE(LPCTSTR a, const V & b) {
    RegSaveValue(REG_CONFIG, a, true, b);
}

std::string RegGetConfigSlotSection(UINT slot);
void RegDeleteConfigSlotSection(UINT slot);
void RegSetConfigSlotNewCardType(UINT slot, SS_CARDTYPE type);
void RegSetConfigGameIOConnectorNewDongleType(UINT slot, DONGLETYPE type);
