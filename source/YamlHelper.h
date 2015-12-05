#pragma once

#include "yaml.h"

#define SS_YAML_KEY_FILEHDR "File_hdr"
#define SS_YAML_KEY_TAG "Tag"
#define SS_YAML_KEY_VERSION "Version"
#define SS_YAML_KEY_UNIT "Unit"
#define SS_YAML_KEY_UNIT_TYPE "Type"
#define SS_YAML_KEY_TYPE "Type"
#define SS_YAML_KEY_CARD "Card"
#define SS_YAML_KEY_STATE "State"

#define SS_YAML_VALUE_AWSS "AppleWin Save State"
#define SS_YAML_VALUE_UNIT_CARD "Card"
#define SS_YAML_VALUE_UNIT_CONFIG "Config"

struct MapValue;
typedef std::map<std::string, MapValue> MapYaml;

struct MapValue
{
	std::string value;
	MapYaml* subMap;
};

class YamlHelper
{
friend class YamlLoadHelper;	// YamlLoadHelper can access YamlHelper's private members

public:
	YamlHelper(void) :
		m_hFile(NULL)
	{
		MakeAsciiToHexTable();
	}

	~YamlHelper(void)
	{
		if (m_hFile)
			fclose(m_hFile);
	}

	int InitParser(const char* pPathname);
	void FinaliseParser(void);

	void GetNextEvent(bool bInMap = false);
	int GetScalar(std::string& scalar);
	void GetListStartEvent(void);
//	void GetListEndEvent(void);
	void GetMapStartEvent(void);
	void GetMapEndEvent(void);
	int GetMapStartOrListEndEvent(void);
	std::string GetMapName(void) { return m_mapName; }

private:
	int ParseMap(MapYaml& mapYaml);
	std::string GetMapValue(MapYaml& mapYaml, const std::string key, bool& bFound);
	void GetMapValueMemory(MapYaml& mapYaml, const LPBYTE pMemBase, const size_t kAddrSpaceSize);
	bool GetSubMap(MapYaml** mapYaml, const std::string key);
	void GetMapRemainder(MapYaml& mapYaml);

	void MakeAsciiToHexTable(void);

	yaml_parser_t m_parser;
	yaml_event_t m_newEvent;

	std::string m_scalarName;

	FILE* m_hFile;
	char m_AsciiToHex[256];

	//

	std::string m_lastMapName;
	std::string m_mapName;

	struct MapState
	{
		std::string name;
		bool isInMap;
	};

	std::stack< MapState > m_stackMapState;

	MapYaml m_mapYaml;
};

// -----

class YamlLoadHelper
{
public:
	YamlLoadHelper(YamlHelper& yamlHelper)
		: m_yamlHelper(yamlHelper),
		  m_pMapYaml(&yamlHelper.m_mapYaml),
		  m_bIteratingOverMap(false)
	{
		if (!m_yamlHelper.ParseMap(yamlHelper.m_mapYaml))
			throw std::string(m_yamlHelper.GetMapName() + ": Failed to parse map");
	}

	~YamlLoadHelper(void)
	{
		m_yamlHelper.GetMapRemainder(m_yamlHelper.m_mapYaml);
	}

	INT GetMapValueINT(const std::string key)
	{
		bool bFound;
		std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
		if (value == "") throw std::string(m_yamlHelper.GetMapName() + ": Missing: " + key);
		return strtol(value.c_str(), NULL, 0);
	}

	UINT GetMapValueUINT(const std::string key)
	{
		bool bFound;
		std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
		if (value == "") throw std::string(m_yamlHelper.GetMapName() + ": Missing: " + key);
		return strtoul(value.c_str(), NULL, 0);
	}

	UINT64 GetMapValueUINT64(const std::string key)
	{
		bool bFound;
		std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
		if (value == "") throw std::string(m_yamlHelper.GetMapName() + ": Missing: " + key);
		return _strtoui64(value.c_str(), NULL, 0);
	}

	bool GetMapValueBool(const std::string key)
	{
		return GetMapValueUINT(key) ? true : false;
	}

	BOOL GetMapValueBOOL(const std::string key)
	{
		return GetMapValueUINT(key) ? TRUE : FALSE;
	}

	std::string GetMapValueSTRING_NoThrow(const std::string& key, bool& bFound)
	{
		std::string value = m_yamlHelper.GetMapValue(*m_pMapYaml, key, bFound);
		return value;
	}

	std::string GetMapValueSTRING(const std::string& key)
	{
		bool bFound;
		std::string value = GetMapValueSTRING_NoThrow(key, bFound);
		if (!bFound) throw std::string(m_yamlHelper.GetMapName() + ": Missing: " + key);
		return value;
	}

	void GetMapValueMemory(const LPBYTE pMemBase, const size_t size)
	{
		m_yamlHelper.GetMapValueMemory(*m_pMapYaml, pMemBase, size);
	}

	bool GetSubMap(const std::string key)
	{
		m_stackMap.push(m_pMapYaml);
		bool res = m_yamlHelper.GetSubMap(&m_pMapYaml, key);
		if (!res)
			m_stackMap.pop();
		return res;
	}

	void PopMap(void)
	{
		if (m_stackMap.empty())
			return;

		m_pMapYaml = m_stackMap.top();
		m_stackMap.pop();
	}

	std::string GetMapNextSlotNumber(void)
	{
		if (!m_bIteratingOverMap)
		{
			m_iter = m_pMapYaml->begin();
			m_bIteratingOverMap = true;
		}

		if (m_iter == m_pMapYaml->end())
		{
			m_bIteratingOverMap = false;
			return "";
		}

		std::string scalar = m_iter->first;
		++m_iter;
		return scalar;
	}

private:
	YamlHelper& m_yamlHelper;
	MapYaml* m_pMapYaml;
	std::stack<MapYaml*> m_stackMap;

	bool m_bIteratingOverMap;
	MapYaml::iterator m_iter;
};

// -----

class YamlSaveHelper
{
friend class Indent;	// Indent can access YamlSaveHelper's private members
public:
	YamlSaveHelper(std::string pathname) :
		m_hFile(NULL),
		m_indent(0)
	{
		m_hFile = fopen(pathname.c_str(), "wt");

		// todo: handle ERROR_ALREADY_EXISTS - ask if user wants to replace existing file
		// - at this point any old file will have been truncated to zero

		if(m_hFile == NULL)
			throw std::string("Save error");

		_tzset();
		time_t ltime;
		time(&ltime);
		char timebuf[26];
		errno_t err = ctime_s(timebuf, sizeof(timebuf), &ltime);	// includes newline at end of string
		fprintf(m_hFile, "# Date-stamp: %s\n", err == 0 ? timebuf : "Error: Datestamp\n\n");

		fprintf(m_hFile, "---\n");

		//

		memset(m_szIndent, ' ', kMaxIndent);
	}

	~YamlSaveHelper()
	{
		if (m_hFile)
		{
			fprintf(m_hFile, "...\n");
			fclose(m_hFile);
		}
	}

	void Save(const char* format, ...);

	class Label
	{
	public:
		Label(YamlSaveHelper& rYamlSaveHelper, const char* format, ...) :
			yamlSaveHelper(rYamlSaveHelper)
		{
			fwrite(yamlSaveHelper.m_szIndent, 1, yamlSaveHelper.m_indent, yamlSaveHelper.m_hFile);

			va_list vl;
			va_start(vl, format);
			vfprintf(yamlSaveHelper.m_hFile, format, vl);
			va_end(vl);

			yamlSaveHelper.m_indent += 2;
			_ASSERT(yamlSaveHelper.m_indent < yamlSaveHelper.kMaxIndent);
		}

		~Label(void)
		{
			yamlSaveHelper.m_indent -= 2;
			_ASSERT(yamlSaveHelper.m_indent >= 0);
		}

		YamlSaveHelper& yamlSaveHelper;
	};

	class Slot : public Label
	{
	public:
		Slot(YamlSaveHelper& rYamlSaveHelper, std::string type, UINT slot, UINT version) :
			Label(rYamlSaveHelper, "%d:\n", slot)
		{
			rYamlSaveHelper.Save("%s: %s\n", SS_YAML_KEY_CARD, type.c_str());
			rYamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_VERSION, version);
		}

		~Slot(void) {}
	};

	void FileHdr(UINT version);
	void UnitHdr(std::string type, UINT version);
	void SaveMapValueMemory(const LPBYTE pMemBase, const UINT uMemSize);
	std::string GetSaveString(const char* pValue);

private:
	FILE* m_hFile;

	int m_indent;
	static const UINT kMaxIndent = 50*2;
	char m_szIndent[kMaxIndent];
};
