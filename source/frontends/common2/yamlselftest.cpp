#include "frontends/common2/yamlmap.h"

#include <fstream>
#include <iostream>

namespace
{

    // ------------------- helpers -------------------

    void writeText(const std::string &path, const std::string &text)
    {
        std::ofstream f(path);
        if (!f)
            throw std::runtime_error("cannot write test file: " + path);
        f << text;
    }

    std::string readText(const std::string &path)
    {
        std::ifstream f(path);
        return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    [[noreturn]] void fail(const std::string &msg)
    {
        std::cerr << "TEST FAILED: " << msg << std::endl;
        std::exit(1);
    }

    void pass(const std::string &msg)
    {
        std::cout << "ok: " << msg << std::endl;
    }

    // ------------------- tests -------------------

    void test_round_trip_simple()
    {
        std::map<std::string, std::map<std::string, std::string>> input{{"section", {{"key", "value"}}}};

        common2::writeMapToYaml("tmp.yaml", input);
        auto output = common2::readMapFromYaml("tmp.yaml");

        if (input != output)
            fail("round trip simple");

        pass("round trip simple");
    }

    void test_round_trip_multiple_sections()
    {
        std::map<std::string, std::map<std::string, std::string>> input{
            {"alpha", {{"a", "1"}, {"b", "2"}}}, {"beta", {{"x", "9"}}}};

        common2::writeMapToYaml("tmp.yaml", input);
        auto output = common2::readMapFromYaml("tmp.yaml");

        if (input != output)
            fail("round trip multiple sections");

        pass("round trip multiple sections");
    }

    void test_empty_section()
    {
        writeText("tmp.yaml", "section: {}\n");

        auto result = common2::readMapFromYaml("tmp.yaml");

        if (result.size() != 1)
            fail("empty section: size");

        if (!result.count("section"))
            fail("empty section: missing key");

        if (!result["section"].empty())
            fail("empty section: not empty");

        pass("empty section");
    }

    void test_empty_root()
    {
        writeText("tmp.yaml", "{}\n");

        auto result = common2::readMapFromYaml("tmp.yaml");

        if (!result.empty())
            fail("empty root");

        pass("empty root");
    }

    void test_reject_deep_nesting()
    {
        writeText("tmp.yaml", R"(
section:
  key:
    sub: value
)");

        try
        {
            common2::readMapFromYaml("tmp.yaml");
            fail("deep nesting not rejected");
        }
        catch (const std::exception &)
        {
            pass("reject deep nesting");
        }
    }

    void test_reject_sequence()
    {
        writeText("tmp.yaml", R"(
section:
  - a
)");

        try
        {
            common2::readMapFromYaml("tmp.yaml");
            fail("sequence not rejected");
        }
        catch (const std::exception &)
        {
            pass("reject sequence");
        }
    }

} // anonymous namespace

// ------------------- main -------------------

int main()
{
    try
    {
        test_round_trip_simple();
        test_round_trip_multiple_sections(); // <-- NEW
        test_empty_section();
        test_empty_root();
        test_reject_deep_nesting();
        test_reject_sequence();
    }
    catch (const std::exception &e)
    {
        fail(std::string("unexpected exception: ") + e.what());
    }

    std::cout << "\nALL TESTS PASSED\n";
    return 0;
}
