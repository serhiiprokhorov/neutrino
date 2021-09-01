#pragma once

#include "neutrino_frames_local.hpp"

namespace neutrino
{
    namespace impl
    {
        struct consumer_t
        {
            virtual ~consumer_t() = default;
            virtual void consume_checkpoint(
                const local::payload::nanoepoch_t::type_t&
                , const local::payload::stream_id_t::type_t&
                , const local::payload::event_id_t::type_t&
            ) {};
            virtual void consume_context(
                const local::payload::nanoepoch_t::type_t&
                , const local::payload::stream_id_t::type_t&
                , const local::payload::event_id_t::type_t&
                , const local::payload::event_type_t::event_types&
            ) {};
        };
    }
}
