#pragma once

#include <stdexcept>
#include <string>

namespace neutrino
{
    namespace configure
    {
        struct unsupported_option : public std::runtime_error {
            unsupported_option(std::string option) : std::runtime_error(option.append(" unsupported")) {}
        };

        struct missing_option : public std::runtime_error {
            missing_option(std::string option) : std::runtime_error(option.append(" is missing")) {}
        };

        struct impossible_option_value : public std::runtime_error {
            impossible_option_value(std::string option) : std::runtime_error(option.append(" impossible option value")) {}
        };
    }
    namespace os 
    {
        struct errno_error : public std::runtime_error
        {
          int errno_captured;
          std::source_location location_captured;
          errno_error(std::source_location loc, int err, const char* what)
          : errno_captured(err), location_captured(loc), std::runtime_error(what)
          {}
        };

        struct errno_error : public std::runtime_error {
            errno_error(const char* where) : std::runtime_error(std::string("errno=").append(std::to_string(errno)).append(where)) {}
        };
    }
}