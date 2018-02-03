/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2015, Tom Charlesworth, Michael Pohoreski

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

#include "StdAfx.h"

#include "Log.h"
#include "YamlHelper.h"

int YamlHelper::InitParser(const char* pPathname)
{
	m_hFile = fopen(pPathname, "r");
	if (m_hFile == NULL)
	{
		return 0;
	}

	if (!yaml_parser_initialize(&m_parser))
	{
		return 0;
	}

	// Note: C/C++ > Pre-Processor: YAML_DECLARE_STATIC;
	yaml_parser_set_input_file(&m_parser, m_hFile);

	return 1;
}

void YamlHelper::FinaliseParser(void)
{
	if (m_hFile)
		fclose(m_hFile);

	m_hFile = NULL;
}

void YamlHelper::GetNextEvent(bool bInMap /*= false*/)
{
	if (!yaml_parser_parse(&m_parser, &m_newEvent))
	{
		//printf("Parser error %d\n", m_parser.error);
		throw std::string("Parser error");
	}
}

int YamlHelper::GetScalar(std::string& scalar)
{
	int res = 1;
	bool bDone = false;

	while (!bDone)
	{
		GetNextEvent();

		switch(m_newEvent.type)
		{
		case YAML_SCALAR_EVENT:
			scalar = m_scalarName = (const char*)m_newEvent.data.scalar.value;
			res = 1;
			bDone = true;
			break;
		case YAML_SEQUENCE_END_EVENT:
			res = 0;
			bDone = true;
			break;
		case YAML_MAPPING_END_EVENT:
			res = 0;
			bDone = true;
			break;
		case YAML_STREAM_END_EVENT:
			res = 0;
			bDone = true;
			break;
		}
	}

	return res;
}

void YamlHelper::GetMapStartEvent(void)
{
	GetNextEvent();

	if (m_newEvent.type != YAML_MAPPING_START_EVENT)
	{
		//printf("Unexpected yaml event (%d)\n", m_newEvent.type);
		throw std::string("Unexpected yaml event");
	}
}

int YamlHelper::ParseMap(MapYaml& mapYaml)
{
	mapYaml.clear();

	const char*& pValue = (const char*&) m_newEvent.data.scalar.value;

	bool bKey = true;
	char* pKey = NULL;
	int res = 1;
	bool bDone = false;

	while (!bDone)
	{
		GetNextEvent(true);

		switch(m_newEvent.type)
		{
		case YAML_STREAM_END_EVENT:
			res = 0;
			bDone = true;
			break;
		case YAML_MAPPING_START_EVENT:
			{
				MapValue mapValue;
				mapValue.value = "";
				mapValue.subMap = new MapYaml;
				mapYaml[std::string(pKey)] = mapValue;
				res = ParseMap(*mapValue.subMap);
				if (!res)
					throw std::string("ParseMap: premature end of file during map parsing");
				bKey = true;	// possibly more key,value pairs in this map
			}
			break;
		case YAML_MAPPING_END_EVENT:
			bDone = true;
			break;
		case YAML_SCALAR_EVENT:
			if (bKey)
			{
				pKey = _strdup(pValue);
			}
			else
			{
				MapValue mapValue;
				mapValue.value = pValue;
				mapValue.subMap = NULL;
				mapYaml[std::string(pKey)] = mapValue;
				free(pKey); pKey = NULL;
			}

			bKey = bKey ? false : true;
			break;
		case YAML_SEQUENCE_START_EVENT:
		case YAML_SEQUENCE_END_EVENT:
			throw std::string("ParseMap: Sequence event unsupported");
		}
	}

	if (pKey)
		free(pKey);

	return res;
}

std::string YamlHelper::GetMapValue(MapYaml& mapYaml, const std::string key, bool& bFound)
{
	MapYaml::const_iterator iter = mapYaml.find(key);
	if (iter == mapYaml.end() || iter->second.subMap != NULL)
	{
		bFound = false;	// not found
		return "";
	}

	std::string value = iter->second.value;

	mapYaml.erase(iter);

	bFound = true;
	return value;
}

bool YamlHelper::GetSubMap(MapYaml** mapYaml, const std::string key)
{
	MapYaml::const_iterator iter = (*mapYaml)->find(key);
	if (iter == (*mapYaml)->end() || iter->second.subMap == NULL)
	{
		return false;	// not found
	}

	*mapYaml = iter->second.subMap;
	return true;
}

void YamlHelper::GetMapRemainder(std::string& mapName, MapYaml& mapYaml)
{
	for (MapYaml::iterator iter = mapYaml.begin(); iter != mapYaml.end(); ++iter)
	{
		if (iter->second.subMap)
		{
			std::string subMapName(iter->first);
			GetMapRemainder(subMapName, *iter->second.subMap);
			delete iter->second.subMap;
		}
		else
		{
			const char* pKey = iter->first.c_str();
			LogOutput("%s: Unknown key (%s)\n", mapName.c_str(), pKey);
			LogFileOutput("%s: Unknown key (%s)\n", mapName.c_str(), pKey);
		}
	}

	mapYaml.clear();
}

//

void YamlHelper::MakeAsciiToHexTable(void)
{
	memset(m_AsciiToHex, -1, sizeof(m_AsciiToHex));

	for (int i = '0'; i<= '9'; i++)
		m_AsciiToHex[i] = i - '0';

	for (int i = 'A'; i<= 'F'; i++)
		m_AsciiToHex[i] = i - 'A' + 0xA;

	for (int i = 'a'; i<= 'f'; i++)
		m_AsciiToHex[i] = i - 'a' + 0xA;
}

void YamlHelper::LoadMemory(MapYaml& mapYaml, const LPBYTE pMemBase, const size_t kAddrSpaceSize)
{
	for (MapYaml::iterator it = mapYaml.begin(); it != mapYaml.end(); ++it)
	{
		const char* pKey = it->first.c_str();
		UINT addr = strtoul(pKey, NULL, 16);
		if (addr >= kAddrSpaceSize)
			throw std::string("Memory: line address too big: " + it->first);

		LPBYTE pDst = (LPBYTE) (pMemBase + addr);
		const LPBYTE pDstEnd = (LPBYTE) (pMemBase + kAddrSpaceSize);

		if (it->second.subMap)
			throw std::string("Memory: unexpected sub-map");

		const char* pValue = it->second.value.c_str();
		size_t len = strlen(pValue);
		if (len & 1)
			throw std::string("Memory: hex data must be an even number of nibbles on line address: " + it->first);

		for (UINT i = 0; i<len; i+=2)
		{
			if (pDst >= pDstEnd)
				throw std::string("Memory: hex data overflowed address space on line address: " + it->first);

			BYTE ah = m_AsciiToHex[ (BYTE)(*pValue++) ];
			BYTE al = m_AsciiToHex[ (BYTE)(*pValue++) ];
			if ((ah | al) & 0x80)
				throw std::string("Memory: hex data contains illegal character on line address: " + it->first);

			*pDst++ = (ah<<4) | al;
		}
	}

	mapYaml.clear();
}

//-------------------------------------

INT YamlLoadHelper::LoadInt(const std::string key)
{
	bool bFound;
	std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
	if (value == "")
	{
		m_bDoGetMapRemainder = false;
		throw std::string(m_currentMapName + ": Missing: " + key);
	}
	return strtol(value.c_str(), NULL, 0);
}

UINT YamlLoadHelper::LoadUint(const std::string key)
{
	bool bFound;
	std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
	if (value == "")
	{
		m_bDoGetMapRemainder = false;
		throw std::string(m_currentMapName + ": Missing: " + key);
	}
	return strtoul(value.c_str(), NULL, 0);
}

UINT64 YamlLoadHelper::LoadUint64(const std::string key)
{
	bool bFound;
	std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
	if (value == "")
	{
		m_bDoGetMapRemainder = false;
		throw std::string(m_currentMapName + ": Missing: " + key);
	}
	return _strtoui64(value.c_str(), NULL, 0);
}

bool YamlLoadHelper::LoadBool(const std::string key)
{
	bool bFound;
	std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
	if (value == "true")
		return true;
	else if (value == "false")
		return false;
	m_bDoGetMapRemainder = false;
	throw std::string(m_currentMapName + ": Missing: " + key);
}

std::string YamlLoadHelper::LoadString_NoThrow(const std::string& key, bool& bFound)
{
	std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
	return value;
}

std::string YamlLoadHelper::LoadString(const std::string& key)
{
	bool bFound;
	std::string value = LoadString_NoThrow(key, bFound);
	if (!bFound)
	{
		m_bDoGetMapRemainder = false;
		throw std::string(m_currentMapName + ": Missing: " + key);
	}
	return value;
}

void YamlLoadHelper::LoadMemory(const LPBYTE pMemBase, const size_t size)
{
	m_yamlHelper.LoadMemory(*m_pMapYaml, pMemBase, size);
}

//-------------------------------------

void YamlSaveHelper::Save(const char* format, ...)
{
	fwrite(m_szIndent, 1, m_indent, m_hFile);

	va_list vl;
	va_start(vl, format);
	vfprintf(m_hFile, format, vl);
	va_end(vl);
}

void YamlSaveHelper::SaveInt(const char* key, int value)
{
	Save("%s: %d\n", key, value);
}

void YamlSaveHelper::SaveUint(const char* key, UINT value)
{
	Save("%s: %u\n", key, value);
}

void YamlSaveHelper::SaveHexUint4(const char* key, UINT value)
{
	Save("%s: 0x%01X\n", key, value);
}

void YamlSaveHelper::SaveHexUint8(const char* key, UINT value)
{
	Save("%s: 0x%02X\n", key, value);
}

void YamlSaveHelper::SaveHexUint12(const char* key, UINT value)
{
	Save("%s: 0x%03X\n", key, value);
}

void YamlSaveHelper::SaveHexUint16(const char* key, UINT value)
{
	Save("%s: 0x%04X\n", key, value);
}

void YamlSaveHelper::SaveHexUint24(const char* key, UINT value)
{
	Save("%s: 0x%06X\n", key, value);
}

void YamlSaveHelper::SaveHexUint32(const char* key, UINT value)
{
	Save("%s: 0x%08X\n", key, value);
}

void YamlSaveHelper::SaveHexUint64(const char* key, UINT64 value)
{
	Save("%s: 0x%016llX\n", key, value);
}

void YamlSaveHelper::SaveBool(const char* key, bool value)
{
	Save("%s: %s\n", key, value ? "true" : "false");
}

void YamlSaveHelper::SaveString(const char* key,  const char* value)
{
	Save("%s: %s\n", key, (value[0] != 0) ? value : "\"\"");
}

// Pre: uMemSize must be multiple of 8
void YamlSaveHelper::SaveMemory(const LPBYTE pMemBase, const UINT uMemSize)
{
	if (uMemSize & 7)
		throw std::string("Memory: size must be multiple of 8");

	const UINT kIndent = m_indent;

	const UINT kStride = 64;
	const char szHex[] = "0123456789ABCDEF"; 

	size_t lineSize = kIndent+6+2*kStride+2;	// "AAAA: 00010203...3F\n\00" = 6+ 2*64 +2
	char* const pLine = new char [lineSize];

	for(DWORD dwOffset = 0x0000; dwOffset < uMemSize; dwOffset+=kStride)
	{
		char* pDst = pLine;
		for (UINT i=0; i<kIndent; i++)
			*pDst++ = ' ';
		*pDst++ = szHex[ (dwOffset>>12)&0xf ];
		*pDst++ = szHex[ (dwOffset>>8)&0xf ];
		*pDst++ = szHex[ (dwOffset>>4)&0xf ];
		*pDst++ = szHex[  dwOffset&0xf ];
		*pDst++ = ':';
		*pDst++ = ' ';

		LPBYTE pMem = pMemBase + dwOffset;

		for (UINT i=0; i<kStride; i+=8)
		{
			if (dwOffset + i >= uMemSize)	// Support short final line (still multiple of 8 bytes)
			{
				lineSize = lineSize - 2*kStride + 2*i;
				break;
			}

			BYTE d;
			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];
			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];
			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];
			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];

			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];
			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];
			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];
			d = *pMem++; *pDst++ = szHex[d>>4]; *pDst++ = szHex[d&0xf];
		}

		*pDst++ = '\n';
		*pDst = 0;	// For debugger

		fwrite(pLine, 1, lineSize-1, m_hFile);	// -1 so don't write null terminator
	}

	delete [] pLine;
}

void YamlSaveHelper::FileHdr(UINT version)
{
	fprintf(m_hFile, "%s:\n", SS_YAML_KEY_FILEHDR);
	m_indent = 2;
	SaveString(SS_YAML_KEY_TAG, SS_YAML_VALUE_AWSS);
	SaveInt(SS_YAML_KEY_VERSION, version);
}

void YamlSaveHelper::UnitHdr(std::string type, UINT version)
{
	fprintf(m_hFile, "\n%s:\n", SS_YAML_KEY_UNIT);
	m_indent = 2;
	SaveString(SS_YAML_KEY_TYPE, type.c_str());
	SaveInt(SS_YAML_KEY_VERSION, version);
}
