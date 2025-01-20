#pragma once

#include <string>
#include <map>
#include <memory>

class Registry
{
public:
  virtual ~Registry() = default;

  static std::shared_ptr<Registry> instance;

  virtual std::string getString(const std::string & section, const std::string & key) const = 0;
  virtual uint32_t getDWord(const std::string & section, const std::string & key) const = 0;
  virtual bool getBool(const std::string & section, const std::string & key) const = 0;

  virtual void putString(const std::string & section, const std::string & key, const std::string & value) = 0;
  virtual void putDWord(const std::string & section, const std::string & key, const uint32_t value) = 0;

  virtual std::map<std::string, std::map<std::string, std::string>> getAllValues() const = 0;

  virtual std::string getLocation() const = 0;

};
