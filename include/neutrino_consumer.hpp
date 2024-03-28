#pragma once

#include "neutrino_types.h"

namespace neutrino
{
    namespace consumer
    {
        namespace configure
        {
            void from_cmd_line_args(int argc, const char* argv[]);
        }
    }

    // represents basic consumer operation; in use by a consumer implementation and by a consumer proxy created at producer's side
    struct consumer_t
    {
        virtual ~consumer_t() = default;

        virtual void consume_checkpoint(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) noexcept {};
        virtual void consume_context_enter(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) noexcept {};
        virtual void consume_context_leave(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) noexcept {};
        virtual void consume_context_exception(
            const neutrino_nanoepoch_t&
            , const neutrino_stream_id_t&
            , const neutrino_event_id_t&
        ) noexcept {};
    };
} // neutrino
