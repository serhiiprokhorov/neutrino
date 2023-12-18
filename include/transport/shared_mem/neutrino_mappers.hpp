#pragma once

#include <neutrino_types.h>
#include <neutrino_consumer.hpp>

namespace neutrino
{
    namespace mappers_v00
    {

        struct checkpoint {
            constexpr const std::size_t bytes() {
                return
                    sizeof(EVENTS::CHECKPOINT) 
                    + sizeof(neutrino_nanoepoch_t) 
                    + sizeof(neutrino_stream_id_t) 
                    + sizeof(neutrino_event_id_t);
            }
        }

        struct context_enter {
            constexpr const std::size_t bytes() {
                return
                    sizeof(EVENTS::CONTEXT_ENTER) 
                    + sizeof(neutrino_nanoepoch_t) 
                    + sizeof(neutrino_stream_id_t) 
                    + sizeof(neutrino_event_id_t);
            }
        }

        struct context_leave {
            constexpr const std::size_t bytes() {
                return
                    sizeof(EVENTS::CONTEXT_LEAVE) 
                    + sizeof(neutrino_nanoepoch_t) 
                    + sizeof(neutrino_stream_id_t) 
                    + sizeof(neutrino_event_id_t);
            }
        }

        struct context_exception {
            constexpr const std::size_t bytes() {
                return
                    sizeof(EVENTS::CONTEXT_EXCEPTION) 
                    + sizeof(neutrino_nanoepoch_t) 
                    + sizeof(neutrino_stream_id_t) 
                    + sizeof(neutrino_event_id_t);
            }
        }

        template <typename TRANSPORT>
        void transport_checkpoint(
                const neutrino_nanoepoch_t* nanoepoch, 
                const neutrino_stream_id_t* stream_id, 
                const neutrino_event_id_t* event_id,
                TRANSPORT& t
            ) {
            constexpr const auto bytes = sizeof(nanoepoch) + sizeof(stream_id) + sizeof(event_id);
            t.region(bytes).add(&stream_id).add(&nanoepoch).add(&event_id).add(EVENT_CHECKPOINT).commit();
        }

        template <typename TRANSPORT>
        void transport_context_enter(
            const neutrino_nanoepoch_t* nanoepoch, 
            const neutrino_stream_id_t* stream_id, 
            const neutrino_event_id_t* event_id,
                TRANSPORT& t
            ) {
            constexpr const auto bytes = sizeof(nanoepoch) + sizeof(stream_id) + sizeof(event_id);
            t.region(bytes).add(&stream_id).add(&nanoepoch).add(&event_id).add(EVENT_CONTEXT_ENTER).commit();
        }

        template <typename TRANSPORT>
        void transport_context_leave(
            const neutrino_nanoepoch_t* nanoepoch, 
            const neutrino_stream_id_t* stream_id, 
            const neutrino_event_id_t* event_id,
                TRANSPORT& t
            ) {
            constexpr const auto bytes = sizeof(nanoepoch) + sizeof(stream_id) + sizeof(event_id);
            t.region(bytes).add(&stream_id).add(&nanoepoch).add(&event_id).add(EVENT_CONTEXT_LEAVE).commit();
        }

        template <typename TRANSPORT>
        void transport_context_exception(
            const neutrino_nanoepoch_t* nanoepoch, 
            const neutrino_stream_id_t* stream_id, 
            const neutrino_event_id_t* event_id,
                TRANSPORT& t
            ) {
            constexpr const auto bytes = sizeof(nanoepoch) + sizeof(stream_id) + sizeof(event_id);
            t.region(bytes).add(&stream_id).add(&nanoepoch).add(&event_id).add(EVENT_CONTEXT_EXCEPTION).commit();
        }

    }
}
