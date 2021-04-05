#include <neutrino_mock.hpp>

namespace std
{
    std::string to_string(neutrino::impl::local::payload::event_type_t::event_types e)
    {
        switch (e)
        {
        case neutrino::impl::local::payload::event_type_t::event_types::NO_CONTEXT:
            return "NO_CONTEXT";
        case neutrino::impl::local::payload::event_type_t::event_types::CONTEXT_ENTER:
            return "ENTER";
        case neutrino::impl::local::payload::event_type_t::event_types::CONTEXT_LEAVE:
            return "LEAVE";
        case neutrino::impl::local::payload::event_type_t::event_types::CONTEXT_PANIC:
            return "PANIC";
        default:
            break;
        }
        return std::to_string(static_cast<unsigned int>(e));
    }
}
