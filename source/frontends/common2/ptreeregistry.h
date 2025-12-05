#pragma once

#include "linux/registryclass.h"

namespace common2
{

    class PTreeRegistry : public Registry
    {
    public:
        std::string getString(const std::string &section, const std::string &key) const override;
        uint32_t getDWord(const std::string &section, const std::string &key) const override;
        bool getBool(const std::string &section, const std::string &key) const override;

        void putString(const std::string &section, const std::string &key, const std::string &value) override;
        void putDWord(const std::string &section, const std::string &key, const uint32_t value) override;

        const std::map<std::string, std::map<std::string, std::string>> &getAllValues() const override;

        std::string getLocation() const override;

        void saveToYamlFile(const std::string &filename) const;

    protected:
        std::map<std::string, std::map<std::string, std::string>> myData;
    };

} // namespace common2
