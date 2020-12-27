#include "StdAfx.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/programoptions.h"

#include "Log.h"
#include "linux/registry.h"
#include "linux/windows/files.h"
#include "frontends/qt/applicationname.h"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <sys/stat.h>

namespace
{

  void parseOption(const std::string & s, std::string & path, std::string & value)
  {
    const size_t pos = s.find('=');
    if (pos == std::string::npos)
    {
      throw std::runtime_error("Invalid option format: " + s + ", expected: section.key=value");
    }
    path = s.substr(0, pos);
    std::replace(path.begin(), path.end(), '_', ' ');
    value = s.substr(pos + 1);
    std::replace(value.begin(), value.end(), '_', ' ');
  }

  struct KeyEncodedLess
  {
    static std::string decodeKey(const std::string & key)
    {
      std::string result = key;
      // quick implementation, just to make it work.
      // is there a library function available somewhere?
      boost::algorithm::replace_all(result, "%20", " ");
      return result;
    }

    bool operator()( const std::string & lhs, const std::string & rhs ) const
    {
      const std::string key1 = decodeKey(lhs);
      const std::string key2 = decodeKey(rhs);
      return key1 < key2;
    }
  };

  class Configuration : public Registry
  {
  public:
    typedef boost::property_tree::basic_ptree<std::string, std::string, KeyEncodedLess> ini_t;

    Configuration(const std::string & filename, const bool saveOnExit);
    ~Configuration();

    static std::shared_ptr<Configuration> instance;

    virtual std::string getString(const std::string & section, const std::string & key) const;
    virtual DWORD getDWord(const std::string & section, const std::string & key) const;
    virtual bool getBool(const std::string & section, const std::string & key) const;

    virtual void putString(const std::string & section, const std::string & key, const std::string & value);
    virtual void putDWord(const std::string & section, const std::string & key, const DWORD value);

    template<typename T>
    T getValue(const std::string & section, const std::string & key) const;

    template<typename T>
    void putValue(const std::string & section, const std::string & key, const T & value);

    const ini_t & getProperties() const;

    void addExtraOptions(const std::vector<std::string> & options);

  private:
    const std::string myFilename;
    const bool mySaveOnExit;

    static std::string decodeKey(const std::string & key);

    ini_t myINI;
  };

  std::shared_ptr<Configuration> Configuration::instance;

  Configuration::Configuration(const std::string & filename, const bool saveOnExit) : myFilename(filename), mySaveOnExit(saveOnExit)
  {
    if (GetFileAttributes(myFilename.c_str()) != INVALID_FILE_ATTRIBUTES)
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
    if (mySaveOnExit)
    {
      boost::property_tree::ini_parser::write_ini(myFilename, myINI);
    }
  }

  const Configuration::ini_t & Configuration::getProperties() const
  {
    return myINI;
  }

  std::string Configuration::getString(const std::string & section, const std::string & key) const
  {
    return getValue<std::string>(section, key);
  }

  DWORD Configuration::getDWord(const std::string & section, const std::string & key) const
  {
    return getValue<DWORD>(section, key);
  }

  bool Configuration::getBool(const std::string & section, const std::string & key) const
  {
    return getValue<bool>(section, key);
  }

  void Configuration::putString(const std::string & section, const std::string & key, const std::string & value)
  {
    putValue(section, key, value);
  }

  void Configuration::putDWord(const std::string & section, const std::string & key, const DWORD value)
  {
    putValue(section, key, value);
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

  void Configuration::addExtraOptions(const std::vector<std::string> & options)
  {
    for (const std::string & option : options)
    {
      std::string path, value;
      parseOption(option, path, value);
      myINI.put(path, value);
    }
  }
}

void InitializeFileRegistry(const EmulatorOptions & options)
{
  const char* homeDir = getenv("HOME");
  if (!homeDir)
  {
    throw std::runtime_error("${HOME} not set, cannot locate configuration file");
  }

  std::string filename;
  bool saveOnExit;

  if (options.useQtIni)
  {
    filename = std::string(homeDir) + "/.config/" + ORGANIZATION_NAME + "/" + APPLICATION_NAME + ".conf";
    saveOnExit = false;
  }
  else
  {
    const std::string dir = std::string(homeDir) + "/.applewin";
    const int status = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    filename = dir + "/applewin.conf";
    if (!status || (errno == EEXIST))
    {
      saveOnExit = options.saveConfigurationOnExit;
    }
    else
    {
      const char * s = strerror(errno);
      LogFileOutput("No registry. Cannot create %s: %s\n", dir.c_str(), s);
      saveOnExit = false;
    }
  }

  std::shared_ptr<Configuration> config(new Configuration(filename, saveOnExit));
  config->addExtraOptions(options.registryOptions);

  Registry::instance = config;
}
