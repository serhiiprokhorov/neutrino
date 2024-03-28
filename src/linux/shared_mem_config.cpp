
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <neutrino_errors.hpp>
#include <neutrino_shared_mem_v00_buffer.hpp>

struct config_value_t {
    const char* m_name = "";
    std::string_view m_value;
    const char* m_description = "";
};

struct config_set_t {
    config_value_t config_arguments[3] = {
            // maintain alphanumeric order !!!
            // use distinct first letter !!!
            { .m_name = nullptr, .m_value="", .m_description = "unknown parameter"},
            { .m_name = "cc_buffers", .m_value="", .m_description = "shared memory size, bytes"},
            { .m_name = "process_kind", .m_value="", .m_description = "consumer|producer"},
            { .m_name = "shm_size", .m_value="", .m_description = "shared memory size, bytes"},
    };

    constexpr config_set_t() = default;

    const config_value_t& get_cc_buffers() const {
        return config_arguments[1];
    } 

    const config_value_t& get_process_kind() const {
        return config_arguments[2];
    } 

    const config_value_t& get_shm_size() const {
        return config_arguments[3];
    } 
};

// this function makes sure config_set_t does not violate preconditions:
// - every config argument's first letter is different
// - config_set_t::config_arguments array is sorted in non-decreased order
// - no two config arguments starts with the same letter
// - config_set_t::config_arguments[0] points to nullptr indicating "missing argument" condition
constexpr bool is_config_set_properly_initialized() {
    config_set_t config_set;

    static_assert( config_set.config_arguments[0] != nullptr, "config_single_value_t config_arguments[0] expect nullptr");

    char first_letter =' ';
    for(std::size_t idx = 1; idx < sizeof(config_set.config_arguments) / sizeof(config_set.config_arguments[0]); idx++) {
        static_assert( config_set.config_arguments[idx].m_name[0] <= first_letter, "config_single_value_t config_arguments violate distinct/non decreasing order");
        first_letter = config_set.config_arguments[idx].m_name[0];
    }
    return true;
}

constexpr static bool config_set_t_properly_initialized = is_config_set_properly_initialized();

// parses cv formatted as "name=value,name=value...,name=value"
std::pair<bool, config_set_t> config_set_from_comma_separated(const std::string_view& cv) {

    std::pair<
        bool, // true when all given arguments were parsed, false when some arguments missing 
        config_set_t // parsed arguments
        > ret;
    ret.first = false;

    struct config_set_parser_t {
        char indexes[255] = {0};
        config_set_parser_t(const config_set_t& s) {
            for(std::size_t idx = 0; idx < sizeof(s.config_arguments) / sizeof(s.config_arguments[0]); idx++) {
                indexes[s.config_arguments[idx].m_name[0]] = idx;
            }
        }
        std::size_t get_idx(const char first_letter_name) const {
            return indexes[first_letter_name[0]];
        }
    } parser(ret.second);

    // parser state: two lexs, two states
    enum class state {
        reading_name, // from begin of string or the most recent , until =
        reading_value // from most recent = to , or end of string
    } state_v = state::read_name;

    std::size_t cfg_idx = 0; // index of config argument

    const auto b = cv.cbegin(); // begin of a current lex
    const auto e = b; // end of a current lex
    while(b != cv.cend())
    {
        if(state_v == state::read_name) {
            if(*e == '=')
            {
                cfg_idx = parser.get_idx(*b);
                if(!cfg_idx) {
                    throw configure::unknown_option(std::source_location::current(), b);
                }
                state_v = state::read_value;
                b = e+1;
            }
        }

        if(state_v == state::read_value) {
            if(*e == ',')
            {
                ret.second.config_arguments[cfg_idx].m_value = std::string_view(b,e);
                state_v = state::read_name;
                b = e+1;
            }
        }
        e++;
    }
    if(state_v == state::read_value) {
        ret.second.config_arguments[cfg_idx].m_value = std::string_view(b,p);
    }
    ret.first = true;
    return ret;
}