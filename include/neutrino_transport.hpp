#pragma once

#include <memory>
#include "neutrino_frames_local.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct endpoint_t
            {
                virtual ~endpoint_t() = default;

                virtual bool consume(const uint8_t*, const uint8_t*) { return false; };
                virtual bool flush() { return false; };
            };

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

            namespace frame_v00
            {
                enum class known_encodings_t
                {
                    BINARY_NETWORK
                    , BINARY_NATIVE // for localhost
                    , JSON
                };
            }
        }
    }
}
