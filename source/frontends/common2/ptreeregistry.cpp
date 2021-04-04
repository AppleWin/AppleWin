#include "frontends/common2/ptreeregistry.h"

#include <boost/algorithm/string/replace.hpp>

namespace
{

  std::string decodeKey(const std::string & key)
  {
    std::string result = key;
    // quick implementation, just to make it work.
    // is there a library function available somewhere?
    boost::algorithm::replace_all(result, "%20", " ");
    return result;
  }

}

namespace common2
{

  bool PTreeRegistry::KeyQtEncodedLess::operator()(const std::string & lhs, const std::string & rhs) const
  {
    const std::string key1 = decodeKey(lhs);
    const std::string key2 = decodeKey(rhs);
    return key1 < key2;
  }

  std::string PTreeRegistry::getString(const std::string & section, const std::string & key) const
  {
    return getValue<std::string>(section, key);
  }

  DWORD PTreeRegistry::getDWord(const std::string & section, const std::string & key) const
  {
    return getValue<DWORD>(section, key);
  }

  bool PTreeRegistry::getBool(const std::string & section, const std::string & key) const
  {
    return getValue<bool>(section, key);
  }

  void PTreeRegistry::putString(const std::string & section, const std::string & key, const std::string & value)
  {
    putValue(section, key, value);
  }

  void PTreeRegistry::putDWord(const std::string & section, const std::string & key, const DWORD value)
  {
    putValue(section, key, value);
  }

  template <typename T>
  T PTreeRegistry::getValue(const std::string & section, const std::string & key) const
  {
    const std::string path = section + "." + key;
    const T value = myINI.get<T>(path);
    return value;
  }

  template <typename T>
  void PTreeRegistry::putValue(const std::string & section, const std::string & key, const T & value)
  {
    const std::string path = section + "." + key;
    myINI.put(path, value);
  }

  std::map<std::string, std::map<std::string, std::string>> PTreeRegistry::getAllValues() const
  {
    std::map<std::string, std::map<std::string, std::string>> values;
    for (const auto & it1 : myINI)
    {
      for (const auto & it2 : it1.second)
      {
        values[it1.first][it2.first] = it2.second.get_value<std::string>();
      }
    }
    return values;
  }

}
