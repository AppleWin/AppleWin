#include "frontends/common2/yamlmap.h"

#include <yaml.h>
#include <stdexcept>
#include <memory>
#include <sstream>

namespace
{
    struct FileCloser
    {
        void operator()(FILE *f) const noexcept
        {
            if (f)
                fclose(f);
        }
    };
    using FilePtr = std::unique_ptr<FILE, FileCloser>;

    struct YamlParser
    {
        yaml_parser_t parser;

        YamlParser()
        {
            if (!yaml_parser_initialize(&parser))
                throw std::runtime_error("yaml_parser_initialize failed");
        }

        ~YamlParser()
        {
            yaml_parser_delete(&parser);
        }

        yaml_parser_t *get() noexcept
        {
            return &parser;
        }

        YamlParser(const YamlParser &) = delete;
        YamlParser &operator=(const YamlParser &) = delete;
    };

    struct YamlEvent
    {
        yaml_event_t event{};
        bool valid = false;

        YamlEvent() = default;

        bool parse(yaml_parser_t *parser)
        {
            valid = yaml_parser_parse(parser, &event);
            return valid;
        }

        ~YamlEvent()
        {
            if (valid)
                yaml_event_delete(&event);
        }

        YamlEvent(const YamlEvent &) = delete;
        YamlEvent &operator=(const YamlEvent &) = delete;
    };

    struct YamlEmitter
    {
        yaml_emitter_t emitter;

        YamlEmitter()
        {
            if (!yaml_emitter_initialize(&emitter))
                throw std::runtime_error("yaml_emitter_initialize failed");
        }

        ~YamlEmitter()
        {
            yaml_emitter_delete(&emitter);
        }

        yaml_emitter_t *get() noexcept
        {
            return &emitter;
        }

        YamlEmitter(const YamlEmitter &) = delete;
        YamlEmitter &operator=(const YamlEmitter &) = delete;
    };

    struct YamlOutEvent
    {
        yaml_event_t event{};
        bool valid = false;

        YamlOutEvent() = default;

        ~YamlOutEvent()
        {
            if (valid)
                yaml_event_delete(&event);
        }

        yaml_event_t *init()
        {
            valid = true;
            return &event;
        }

        YamlOutEvent(const YamlOutEvent &) = delete;
        YamlOutEvent &operator=(const YamlOutEvent &) = delete;
    };

} // namespace

namespace common2
{

    void writeMapToYaml(
        const std::string &filename, const std::map<std::string, std::map<std::string, std::string>> &data)
    {
        FilePtr file(fopen(filename.c_str(), "w"));
        if (!file)
            throw std::runtime_error("Cannot open file for writing: " + filename);

        YamlEmitter emitter;
        yaml_emitter_set_output_file(emitter.get(), file.get());

        auto emit = [&](YamlOutEvent &ev)
        {
            if (!yaml_emitter_emit(emitter.get(), ev.init()))
                throw std::runtime_error("YAML emitter error");
            ev.valid = false; // ownership transferred
        };

        {
            YamlOutEvent ev;
            yaml_stream_start_event_initialize(ev.init(), YAML_UTF8_ENCODING);
            emit(ev);
        }

        {
            YamlOutEvent ev;
            yaml_document_start_event_initialize(ev.init(), nullptr, nullptr, nullptr, 0);
            emit(ev);
        }

        {
            YamlOutEvent ev;
            yaml_mapping_start_event_initialize(ev.init(), nullptr, nullptr, 1, YAML_BLOCK_MAPPING_STYLE);
            emit(ev);
        }

        for (const auto &section : data)
        {
            {
                YamlOutEvent ev;
                yaml_scalar_event_initialize(
                    ev.init(), nullptr, nullptr, (yaml_char_t *)section.first.c_str(), section.first.size(), 1, 1,
                    YAML_PLAIN_SCALAR_STYLE);
                emit(ev);
            }

            {
                YamlOutEvent ev;
                yaml_mapping_start_event_initialize(ev.init(), nullptr, nullptr, 1, YAML_BLOCK_MAPPING_STYLE);
                emit(ev);
            }

            for (const auto &kv : section.second)
            {
                {
                    YamlOutEvent ev;
                    yaml_scalar_event_initialize(
                        ev.init(), nullptr, nullptr, (yaml_char_t *)kv.first.c_str(), kv.first.size(), 1, 1,
                        YAML_PLAIN_SCALAR_STYLE);
                    emit(ev);
                }

                {
                    YamlOutEvent ev;
                    yaml_scalar_event_initialize(
                        ev.init(), nullptr, nullptr, (yaml_char_t *)kv.second.c_str(), kv.second.size(), 1, 1,
                        YAML_PLAIN_SCALAR_STYLE);
                    emit(ev);
                }
            }

            {
                YamlOutEvent ev;
                yaml_mapping_end_event_initialize(ev.init());
                emit(ev);
            }
        }

        {
            YamlOutEvent ev;
            yaml_mapping_end_event_initialize(ev.init());
            emit(ev);
        }

        {
            YamlOutEvent ev;
            yaml_document_end_event_initialize(ev.init(), 1);
            emit(ev);
        }

        {
            YamlOutEvent ev;
            yaml_stream_end_event_initialize(ev.init());
            emit(ev);
        }
    }

    // Accepted YAML grammar:
    // root := mapping<string, mapping<string, string>>
    // - exactly two mapping levels
    // - scalars only
    // - sequences, aliases, tags rejected
    std::map<std::string, std::map<std::string, std::string>> readMapFromYaml(const std::string &filename)
    {
        FilePtr file(fopen(filename.c_str(), "r"));
        if (!file)
            throw std::runtime_error("Cannot open file: " + filename);

        YamlParser parser;
        yaml_parser_set_input_file(parser.get(), file.get());

        enum class State
        {
            ExpectStreamStart,
            ExpectDocumentStart,
            ExpectRootMapping,
            ExpectSectionKey,
            ExpectSectionMap,
            ExpectInnerKey,
            ExpectInnerValue,
            Done
        };

        State state = State::ExpectStreamStart;

        std::map<std::string, std::map<std::string, std::string>> result;
        std::string currentSection;
        std::string currentKey;

        auto error = [&](const YamlEvent &ev, const std::string &msg)
        {
            std::ostringstream ss;
            ss << msg << " @ " << filename << ":" << ev.event.start_mark.line + 1 << ":"
               << ev.event.start_mark.column + 1;
            throw std::runtime_error(ss.str());
        };

        while (true)
        {
            YamlEvent ev;
            if (!ev.parse(parser.get()))
                break;

            if (parser.get()->error != YAML_NO_ERROR)
                error(ev, parser.get()->problem ? parser.get()->problem : "YAML parser error");

            const auto &e = ev.event;

            switch (e.type)
            {

            case YAML_STREAM_START_EVENT:
                if (state != State::ExpectStreamStart)
                    error(ev, "Unexpected stream start");
                state = State::ExpectDocumentStart;
                break;

            case YAML_DOCUMENT_START_EVENT:
                if (state != State::ExpectDocumentStart)
                    error(ev, "Unexpected document start");
                state = State::ExpectRootMapping;
                break;

            case YAML_MAPPING_START_EVENT:
                if (state == State::ExpectRootMapping)
                {
                    state = State::ExpectSectionKey;
                }
                else if (state == State::ExpectSectionMap)
                {
                    state = State::ExpectInnerKey;
                }
                else
                {
                    error(ev, "Unexpected mapping start");
                }
                break;

            case YAML_MAPPING_END_EVENT:
                if (state == State::ExpectInnerKey)
                {
                    state = State::ExpectSectionKey;
                }
                else if (state == State::ExpectSectionKey)
                {
                    state = State::Done;
                }
                else
                {
                    error(ev, "Unexpected mapping end");
                }
                break;

            case YAML_SCALAR_EVENT:
            {
                if (!e.data.scalar.value)
                    error(ev, "Null scalar");

                std::string value(reinterpret_cast<const char *>(e.data.scalar.value), e.data.scalar.length);

                switch (state)
                {
                case State::ExpectSectionKey:
                    currentSection = value;
                    result[currentSection];
                    state = State::ExpectSectionMap;
                    break;

                case State::ExpectInnerKey:
                    currentKey = value;
                    state = State::ExpectInnerValue;
                    break;

                case State::ExpectInnerValue:
                    result[currentSection][currentKey] = value;
                    state = State::ExpectInnerKey;
                    break;

                default:
                    error(ev, "Unexpected scalar");
                }
                break;
            }

            case YAML_DOCUMENT_END_EVENT:
                break;

            case YAML_STREAM_END_EVENT:
                return result;

            default:
                error(ev, "Unsupported YAML construct");
            }
        }

        throw std::runtime_error("Unexpected end of YAML stream");
    }

} // namespace common2
