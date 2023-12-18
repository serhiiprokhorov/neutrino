#pragma once

#include "neutrino_types.h"

namespace neutrino
{
    // represents basic consumer operation; in use by a consumer implementation and by a consumer proxy created at producer's side
    struct consumer_t
    {
        virtual ~consumer_t() = default;

        virtual void consume_checkpoint(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) {};
        virtual void consume_context_enter(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) {};
        virtual void consume_context_leave(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) {};
        virtual void consume_context_exception(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) {};
    };
} // neutrino
