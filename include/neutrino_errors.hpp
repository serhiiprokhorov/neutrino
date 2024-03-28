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
            template <typename O>
            unsupported_option(const std::source_location loc, O option) 
            : std::runtime_error(to_string(loc).append(" unsupported option:").append(option)) {}
        };

        struct missing_option : public std::runtime_error {
            template <typename O>
            missing_option(const std::source_location loc, O option) 
            : std::runtime_error(to_string(loc).append(" missing option:").append(option)) {}
        };

        struct missing_env_variable : public std::runtime_error {
            template <typename O>
            missing_env_variable(const std::source_location loc, O option) 
            : std::runtime_error(to_string(loc).append(" missing env variable:").append(option)) {}
        };

        struct impossible_env_variable_value : public std::runtime_error {
            template <typename O>
            missing_env_variable(const std::source_location loc, O option, O value) 
            : std::runtime_error(to_string(loc).append(" impossible env variable value:").append(option).append("=\"").append(value).append("\"")) {}
        };

        struct impossible_option_value : public std::runtime_error {
            template <typename O>
            impossible_option_value(const std::source_location loc, O option) 
            : std::runtime_error(to_string(loc).append(" impossible value of option:").append(option)) {}
        };

        struct not_a_number_option_value : public impossible_option_value {
            template <typename O>
            not_a_number_option_value(const std::source_location loc, O option)
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