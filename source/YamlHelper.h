#pragma once

#include "yaml.h"

#define SS_YAML_KEY_FILEHDR "File_hdr"
#define SS_YAML_KEY_TAG "Tag"
#define SS_YAML_KEY_VERSION "Version"
#define SS_YAML_KEY_UNIT "Unit"
#define SS_YAML_KEY_TYPE "Type"
#define SS_YAML_KEY_CARD "Card"
#define SS_YAML_KEY_STATE "State"

#define SS_YAML_VALUE_AWSS "AppleWin Save State"

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
		memset(&m_parser, 0, sizeof(m_parser));
		memset(&m_newEvent, 0, sizeof(m_newEvent));
		MakeAsciiToHexTable();
	}

	~YamlHelper(void)
	{
		FinaliseParser();
	}

	int InitParser(const char* pPathname);
	void FinaliseParser(void);

	int GetScalar(std::string& scalar);
	void GetMapStartEvent(void);

private:
	void GetNextEvent(void);
	int ParseMap(MapYaml& mapYaml);
	std::string GetMapValue(MapYaml& mapYaml, const std::string &key, bool& bFound);
	UINT LoadMemory(MapYaml& mapYaml, const LPBYTE pMemBase, const size_t kAddrSpaceSize);
	bool GetSubMap(MapYaml** mapYaml, const std::string &key);
	void GetMapRemainder(std::string& mapName, MapYaml& mapYaml);

	void MakeAsciiToHexTable(void);

	yaml_parser_t m_parser;
	yaml_event_t m_newEvent;

	std::string m_scalarName;

	FILE* m_hFile;
	char m_AsciiToHex[256];

	MapYaml m_mapYaml;
};

// -----

class YamlLoadHelper
{
public:
	YamlLoadHelper(YamlHelper& yamlHelper)
		: m_yamlHelper(yamlHelper),
		  m_pMapYaml(&yamlHelper.m_mapYaml),
		  m_bDoGetMapRemainder(true),
		  m_topLevelMapName(yamlHelper.m_scalarName),
		  m_currentMapName(m_topLevelMapName),
		  m_bIteratingOverMap(false)
	{
		if (!m_yamlHelper.ParseMap(yamlHelper.m_mapYaml))
		{
			m_bDoGetMapRemainder = false;
			throw std::string(m_currentMapName + ": Failed to parse map");
		}
	}

	~YamlLoadHelper(void)
	{
		if (m_bDoGetMapRemainder)
			m_yamlHelper.GetMapRemainder(m_topLevelMapName, m_yamlHelper.m_mapYaml);
	}

	INT LoadInt(const std::string key);
	UINT LoadUint(const std::string key);
	UINT64 LoadUint64(const std::string key);
	bool LoadBool(const std::string key);
	std::string LoadString_NoThrow(const std::string& key, bool& bFound);
	std::string LoadString(const std::string& key);
	float LoadFloat(const std::string & key);
	double LoadDouble(const std::string & key);
	void LoadMemory(const LPBYTE pMemBase, const size_t size);
	void LoadMemory(std::vector<BYTE>& memory, const size_t size);

	bool GetSubMap(const std::string & key)
	{
		YamlStackItem item = {m_pMapYaml, m_currentMapName};
		m_stackMap.push(item);
		bool res = m_yamlHelper.GetSubMap(&m_pMapYaml, key);
		if (!res)
			m_stackMap.pop();
		else
			m_currentMapName = key;
		return res;
	}

	void PopMap(void)
	{
		if (m_stackMap.empty())
			return;

		YamlStackItem item = m_stackMap.top();
		m_stackMap.pop();

		m_pMapYaml = item.pMapYaml;
		m_currentMapName = item.mapName;
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
	bool m_bDoGetMapRemainder;

	struct YamlStackItem
	{
		MapYaml* pMapYaml;
		std::string mapName;
	};
	std::stack<YamlStackItem> m_stackMap;

	std::string m_topLevelMapName;
	std::string m_currentMapName;

	bool m_bIteratingOverMap;
	MapYaml::iterator m_iter;
};

// -----

class YamlSaveHelper
{
public:
	YamlSaveHelper(const std::string & pathname) :
		m_hFile(NULL),
		m_indent(0),
		m_pWcStr(NULL),
		m_wcStrSize(0),
		m_pMbStr(NULL),
		m_mbStrSize(0)
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

		delete[] m_pWcStr;
		delete[] m_pMbStr;
	}

	void Save(const char* format, ...);

	void SaveInt(const char* key, int value);
	void SaveUint(const char* key, UINT value);
	void SaveHexUint4(const char* key, UINT value);
	void SaveHexUint8(const char* key, UINT value);
	void SaveHexUint12(const char* key, UINT value);
	void SaveHexUint16(const char* key, UINT value);
	void SaveHexUint24(const char* key, UINT value);
	void SaveHexUint32(const char* key, UINT value);
	void SaveHexUint64(const char* key, UINT64 value);
	void SaveBool(const char* key, bool value);
	void SaveString(const char* key, const char* value);
	void SaveString(const char* key, const std::string & value);
	void SaveFloat(const char* key, float value);
	void SaveDouble(const char* key, double value);
	void SaveMemory(const LPBYTE pMemBase, const UINT uMemSize);

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
		Slot(YamlSaveHelper& rYamlSaveHelper, const std::string & type, UINT slot, UINT version) :
			Label(rYamlSaveHelper, "%d:\n", slot)
		{
			rYamlSaveHelper.Save("%s: %s\n", SS_YAML_KEY_CARD, type.c_str());
			rYamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_VERSION, version);
		}

		~Slot(void) {}
	};

	void FileHdr(UINT version);
	void UnitHdr(const std::string & type, UINT version);

private:
	FILE* m_hFile;

	int m_indent;
	static const UINT kMaxIndent = 50*2;
	char m_szIndent[kMaxIndent];

	LPWSTR m_pWcStr;
	int m_wcStrSize;
	LPSTR m_pMbStr;
	int m_mbStrSize;
};
