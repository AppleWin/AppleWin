#include "frontends/ncurses/configuration.h"
#include "linux/wincompat.h"

#include "Log.h"

#include <boost/property_tree/ini_parser.hpp>
#include <filesystem>

class Configuration
{
 public:
  Configuration(const std::string & filename);
  ~Configuration();

  static std::shared_ptr<Configuration> instance;

  template<typename T>
  T getValue(const std::string & section, const std::string & key) const;

  template<typename T>
  void putValue(const std::string & section, const std::string & key, const T & value);

  const boost::property_tree::ptree & getProperties() const;

 private:
  const std::string myFilename;

  boost::property_tree::ptree myINI;
};

std::shared_ptr<Configuration> Configuration::instance;

Configuration::Configuration(const std::string & filename) : myFilename(filename)
{
  if (std::filesystem::exists(filename))
  {
    boost::property_tree::ini_parser::read_ini(myFilename, myINI);
  }
  else
  {
    LogFileOutput("Registry: configuration file '%s' not found\n", filename.c_str());
  }
}

Configuration::~Configuration()
{
  boost::property_tree::ini_parser::write_ini(myFilename, myINI);
}

const boost::property_tree::ptree & Configuration::getProperties() const
{
  return myINI;
}

template <typename T>
T Configuration::getValue(const std::string & section, const std::string & key) const
{
  const std::string path = section + "." + key;
  const T value = myINI.get<T>(path);
  return value;
}

template <typename T>
void Configuration::putValue(const std::string & section, const std::string & key, const T & value)
{
  const std::string path = section + "." + key;
  myINI.put(path, value);
}

void InitializeRegistry(const std::string & filename)
{
  Configuration::instance.reset(new Configuration(filename));
}

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser,
                    LPTSTR buffer, DWORD chars)
{
  BOOL result;
  try
  {
    const std::string s = Configuration::instance->getValue<std::string>(section, key);
    strncpy(buffer, s.c_str(), chars);
    buffer[chars - 1] = 0;
    result = TRUE;
    LogFileOutput("RegLoadString: %s - %s = %s\n", section, key, buffer);
  }
  catch (const std::exception & e)
  {
    result = FALSE;
    LogFileOutput("RegLoadString: %s - %s = ??\n", section, key);
  }
  return result;
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value)
{
  BOOL result;
  try
  {
    *value = Configuration::instance->getValue<DWORD>(section, key);
    result = TRUE;
    LogFileOutput("RegLoadValue: %s - %s = %d\n", section, key, *value);
  }
  catch (const std::exception & e)
  {
    result = FALSE;
    LogFileOutput("RegLoadValue: %s - %s = ??\n", section, key);
  }
  return result;
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value)
{
  BOOL result;
  try
  {
    *value = Configuration::instance->getValue<BOOL>(section, key);
    result = TRUE;
    LogFileOutput("RegLoadValue: %s - %s = %d\n", section, key, *value);
  }
  catch (const std::exception & e)
  {
    result = FALSE;
    LogFileOutput("RegLoadValue: %s - %s = ??\n", section, key);
  }
  return result;
}

void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer)
{
  Configuration::instance->putValue(section, key, buffer);
  LogFileOutput("RegSaveString: %s - %s = %s\n", section, key, buffer.c_str());
}

void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value)
{
  Configuration::instance->putValue(section, key, value);
  LogFileOutput("RegSaveValue: %s - %s = %d\n", section, key, value);
}

const boost::property_tree::ptree & getProperties()
{
  return Configuration::instance->getProperties();
}
