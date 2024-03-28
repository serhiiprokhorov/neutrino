#include "neutrino_config.hpp"

namespace neutrino::configure
{
void from_comma_separated_string(const std::string_view& comma_separated, const config_value_t* first, const std::size_t cc)
{
    // struct config_value_callback_t {
    //     const char* m_name = "";
    //     const char* m_description = "";
    //     std::function<void(const std::string_view& value)> m_callback;
    // };

    struct config_set_indexer_t {
        char indexes[255] = {0};
        config_set_indexer_t(const config_value_t* first, const std::size_t cc) {
            for(std::size_t idx = 0; idx < cc; idx++) {
                indexes[first[idx].m_name[0]] = idx;
            }
        }
        std::size_t get_idx(const char first_letter_name) const {
            return indexes[first_letter_name[0]];
        }
    } indexer(first, cc);

    // parser state: two lexs, two states
    enum class state {
        reading_name, // from begin of string or the most recent , until =
        reading_value // from most recent = to , or end of string
    } state_v = state::read_name;

    std::size_t cfg_idx = 0; // index of config argument

    const auto b = comma_separated.cbegin(); // begin of a current lex
    const auto e = b; // end of a current lex
    while(b != comma_separated.cend())
    {
        if(state_v == state::read_name) {
            if(*e == '=')
            {
                cfg_idx = indexer.get_idx(*b);
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
                if(!first[cgf_idx].m_callback_set_value(std::string_view(b,e))) {
                    throw configure::impossible_option_value(std::source_location::current(), first[cgf_idx].m_name);
                }
                state_v = state::read_name;
                b = e+1;
            }
        }
        e++;
    }
    if(state_v == state::read_value) {
        if(!first[cgf_idx].m_callback_set_value(std::string_view(b,p))) {
            throw configure::impossible_option_value(std::source_location::current(), first[cgf_idx].m_name);
        }
    }
}
}
