#pragma once

#include <stdexcept>
#include <source_location>
#include <string>
#include <cstring>

namespace neutrino
{
    inline std::string to_string(const std::source_location& s) {
        return std::string(s.function_name()).append(":").append(std::to_string(s.line()));
    }
    namespace configure
    {
        struct unsupported_option : public std::runtime_error {
            unsupported_option(const std::source_location loc, std::string option) 
            : std::runtime_error(to_string(loc).append(" unsupported option:").append(option)) {}
        };

        struct missing_option : public std::runtime_error {
            missing_option(const std::source_location loc, std::string option) 
            : std::runtime_error(to_string(loc).append(" missing option:").append(option)) {}
        };

        struct impossible_option_value : public std::runtime_error {
            impossible_option_value(const std::source_location loc, std::string option) 
            : std::runtime_error(to_string(loc).append(" impossible value of option:").append(option)) {}
        };

        struct not_a_number_option_value : public impossible_option_value {
            not_a_number_option_value(const std::source_location loc, std::string option)
            : impossible_option_value(loc, std::string("format \"").append(option).append("{value},\"")) {}
        };
    }
    namespace os 
    {
        struct errno_error : public std::runtime_error
        {
          errno_error(std::source_location loc, const char* what)
          : std::runtime_error(to_string(loc).append(" errno:\"").append((const char*)strerror(errno)).append("\":").append(std::to_string(errno)).append(" ").append(what)) {}
        };
    }
}