#include "frontends/common2/yamlmap.h"

#include <yaml.h>
#include <stdexcept>
#include <memory>
#include <string_view>

namespace
{
    // RAII wrapper for FILE*
    struct FileDeleter
    {
        void operator()(FILE *f) const
        {
            if (f)
                fclose(f);
        }
    };

    using FilePtr = std::unique_ptr<FILE, FileDeleter>;

    // RAII wrapper for yaml_parser_t
    struct YamlParserDeleter
    {
        void operator()(yaml_parser_t *p) const
        {
            if (p)
                yaml_parser_delete(p);
        }
    };

    using YamlParserPtr = std::unique_ptr<yaml_parser_t, YamlParserDeleter>;

    enum class YamlParseState
    {
        ExpectSectionKey,
        ExpectSectionValue,
        ExpectKey,
        ExpectValue
    };
} // namespace

namespace common2
{
    // Write nested map to YAML file
    void writeMapToYaml(
        const std::string &filename, const std::map<std::string, std::map<std::string, std::string>> &data)
    {
        FilePtr file(fopen(filename.c_str(), "w"));
        if (!file)
            throw std::runtime_error("Cannot open file for writing: " + filename);

        yaml_emitter_t emitter;
        yaml_emitter_initialize(&emitter);
        yaml_emitter_set_output_file(&emitter, file.get());

        yaml_event_t event;

        // Stream start
        yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
        yaml_emitter_emit(&emitter, &event);

        // Document start
        yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
        yaml_emitter_emit(&emitter, &event);

        // Mapping start (root)
        yaml_mapping_start_event_initialize(
            &event, NULL, (yaml_char_t *)"tag:yaml.org,2002:map", 1, YAML_BLOCK_MAPPING_STYLE);
        yaml_emitter_emit(&emitter, &event);

        // Emit each section
        for (const auto &section : data)
        {
            // Section key
            yaml_scalar_event_initialize(
                &event, NULL, (yaml_char_t *)"tag:yaml.org,2002:str", (yaml_char_t *)section.first.c_str(),
                section.first.size(), 1, 1, YAML_PLAIN_SCALAR_STYLE);
            yaml_emitter_emit(&emitter, &event);

            // Section value (nested mapping)
            yaml_mapping_start_event_initialize(
                &event, NULL, (yaml_char_t *)"tag:yaml.org,2002:map", 1, YAML_BLOCK_MAPPING_STYLE);
            yaml_emitter_emit(&emitter, &event);

            for (const auto &kv : section.second)
            {
                // Key
                yaml_scalar_event_initialize(
                    &event, NULL, (yaml_char_t *)"tag:yaml.org,2002:str", (yaml_char_t *)kv.first.c_str(),
                    kv.first.size(), 1, 1, YAML_PLAIN_SCALAR_STYLE);
                yaml_emitter_emit(&emitter, &event);

                // Value
                yaml_scalar_event_initialize(
                    &event, NULL, (yaml_char_t *)"tag:yaml.org,2002:str", (yaml_char_t *)kv.second.c_str(),
                    kv.second.size(), 1, 1, YAML_PLAIN_SCALAR_STYLE);
                yaml_emitter_emit(&emitter, &event);
            }

            // Section mapping end
            yaml_mapping_end_event_initialize(&event);
            yaml_emitter_emit(&emitter, &event);
        }

        // Root mapping end
        yaml_mapping_end_event_initialize(&event);
        yaml_emitter_emit(&emitter, &event);

        // Document end
        yaml_document_end_event_initialize(&event, 1);
        yaml_emitter_emit(&emitter, &event);

        // Stream end
        yaml_stream_end_event_initialize(&event);
        yaml_emitter_emit(&emitter, &event);

        yaml_emitter_delete(&emitter);
        // file auto-closes via FilePtr destructor
    }

    // Read nested map from YAML file
    std::map<std::string, std::map<std::string, std::string>> readMapFromYaml(const std::string &filename)
    {
        FilePtr file(fopen(filename.c_str(), "r"));
        if (!file)
            throw std::runtime_error("Cannot open file for reading: " + filename);

        YamlParserPtr parser(new yaml_parser_t());
        yaml_parser_initialize(parser.get());
        yaml_parser_set_input_file(parser.get(), file.get());

        yaml_event_t event;
        std::map<std::string, std::map<std::string, std::string>> result;
        std::string currentSection, currentKey;
        YamlParseState state = YamlParseState::ExpectSectionKey;

        while (yaml_parser_parse(parser.get(), &event))
        {
            // Check for parser errors
            if (parser->error != YAML_NO_ERROR)
                throw std::runtime_error(std::string("YAML parser error: ") + parser->problem);

            if (event.type == YAML_SCALAR_EVENT)
            {
                // Check for null scalar value
                if (!event.data.scalar.value)
                    throw std::runtime_error("Encountered null scalar in YAML");

                std::string_view scalar((const char *)event.data.scalar.value, event.data.scalar.length);

                switch (state)
                {
                case YamlParseState::ExpectSectionKey:
                    currentSection = std::string(scalar);
                    state = YamlParseState::ExpectSectionValue;
                    break;

                case YamlParseState::ExpectSectionValue:
                    state = YamlParseState::ExpectKey;
                    break;

                case YamlParseState::ExpectKey:
                    currentKey = std::string(scalar);
                    state = YamlParseState::ExpectValue;
                    break;

                case YamlParseState::ExpectValue:
                    result[currentSection][currentKey] = std::string(scalar);
                    state = YamlParseState::ExpectKey;
                    break;
                }
            }
            else if (event.type == YAML_MAPPING_START_EVENT)
            {
                if (state == YamlParseState::ExpectSectionValue)
                    state = YamlParseState::ExpectKey;
            }
            else if (event.type == YAML_MAPPING_END_EVENT)
            {
                if (state == YamlParseState::ExpectKey)
                    state = YamlParseState::ExpectSectionKey;
            }
            else if (event.type == YAML_STREAM_START_EVENT || event.type == YAML_DOCUMENT_START_EVENT)
            {
                // Ignored: stream/document start events are structural and carry no scalar data
            }
            else if (event.type == YAML_DOCUMENT_END_EVENT)
            {
                // Document end: nothing to do, continue parsing (there may be a stream end after)
            }
            else if (event.type == YAML_STREAM_END_EVENT)
            {
                yaml_event_delete(&event);
                break;
            }
            else
            {
                // Unexpected event type
                throw std::runtime_error(std::string("Unexpected YAML event type: ") + std::to_string(event.type));
            }

            yaml_event_delete(&event);
        }

        // Final check: parser completed without error
        if (parser->error != YAML_NO_ERROR)
            throw std::runtime_error(std::string("YAML parser error: ") + parser->problem);

        // parser and file auto-close via their destructors

        return result;
    }
} // namespace common2
