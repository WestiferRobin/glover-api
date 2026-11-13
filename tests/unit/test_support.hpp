#ifndef GLOVER_TEST_SUPPORT_HPP
#define GLOVER_TEST_SUPPORT_HPP

#include <array>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace test {
class Failure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void fail(const std::string& message,
                 std::source_location where = std::source_location::current()) {
    throw Failure(std::string(where.file_name()) + ':' + std::to_string(where.line()) +
                  ": " + message);
}

template<class T>
std::string show(const T& value) {
    std::ostringstream output;
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        output << '"';
        for (unsigned char byte : std::string_view(value)) {
            switch (byte) {
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            default:
                if (byte < 32 || byte >= 127)
                    output << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                           << static_cast<unsigned>(byte) << std::dec;
                else output << static_cast<char>(byte);
            }
        }
        output << '"';
    } else if constexpr (std::is_enum_v<T>) {
        output << static_cast<std::underlying_type_t<T>>(value);
    } else if constexpr (requires { output << value; }) {
        output << std::boolalpha << value;
    } else if constexpr (requires { value.begin(); value.end(); }) {
        output << '[';
        bool first = true;
        for (const auto& item : value) {
            if (!first) output << ',';
            output << show(item);
            first = false;
        }
        output << ']';
    } else {
        static_assert(sizeof(T) == 0, "Add a diagnostic formatter for this type");
    }
    return output.str();
}

template<class Actual, class Expected>
void equal(const Actual& actual, const Expected& expected,
           std::source_location where = std::source_location::current()) {
    if (!(actual == expected))
        fail("expected " + show(expected) + ", actual " + show(actual), where);
}

inline void check(bool condition, const std::string& description,
                  std::source_location where = std::source_location::current()) {
    if (!condition) fail(description + " (expected true, actual false)", where);
}

template<class Exception, class Function>
void throws(Function&& operation,
            std::source_location where = std::source_location::current()) {
    static_assert(std::is_base_of_v<std::exception, Exception>);
    try {
        operation();
    } catch (const Exception&) {
        return;
    } catch (const std::exception& error) {
        fail("expected exception category did not match: " + std::string(error.what()), where);
    } catch (...) {
        fail("expected standard exception, actual non-standard exception", where);
    }
    fail("expected exception, actual normal return", where);
}

class Runner {
    std::size_t passed = 0, failed = 0;
public:
    const std::filesystem::path fixtures;
    explicit Runner(std::filesystem::path directory) : fixtures(std::move(directory)) {}

    template<class Function>
    void run(const std::string& name, Function&& operation,
             std::source_location where = std::source_location::current()) {
        try {
            operation();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "FAIL " << name << " at " << where.file_name() << ':'
                      << where.line() << ": " << error.what() << '\n';
        } catch (...) {
            ++failed;
            std::cout << "FAIL " << name << " at " << where.file_name() << ':'
                      << where.line() << ": unexpected non-standard exception\n";
        }
    }
    int finish() const {
        std::cout << "TOTAL " << passed + failed << ": " << passed << " passed, "
                  << failed << " failed, 0 skipped\n";
        return failed == 0 ? 0 : 1;
    }
};

void glove(Runner&);
void pair(Runner&);
void mock(Runner&);
void stateCodec(Runner&);
void udpEnvelope(Runner&);
void framing(Runner&);
void gloveJson(Runner&);
void parser(Runner&);
} // namespace test
#endif
