#pragma once

#include <stdint.h>
#include <stdexcept>
#include "neutrino_producer_interface.h"

namespace neutrino
{
    namespace helpers
    {
        struct context_t
        {
            int64_t m_event_id;
            int64_t m_stream_id;
#ifdef UT
            uint64_t m_enter_nanoepoch = 0;
            uint64_t m_exit_nanoepoch = 0;
#endif
#ifdef __cplusplus >= 201703L
            int m_count = std::uncaught_exceptions();
#endif
            context_t(
                uint64_t stream_id
                , uint64_t event_id
#ifdef UT
                , uint64_t enter_nanoepoch = 0
                , uint64_t exit_nanoepoch = 0
#endif
            )
                : m_event_id(event_id), m_stream_id(stream_id)
#ifdef UT
                , m_enter_nanoepoch(enter_nanoepoch)
                , m_exit_nanoepoch(exit_nanoepoch)
#endif
            {
                neutrino_context_enter(
#ifdef UT
                    m_enter_nanoepoch == 0 ? neutrino_nanoepoch() : m_enter_nanoepoch
#else
                    neutrino_nanoepoch()
#endif
                    , stream_id, event_id);
            }
            ~context_t()
            {
                uint64_t ne = 
#ifdef UT
                    m_exit_nanoepoch == 0 ? neutrino_nanoepoch() : m_exit_nanoepoch
#else
                    neutrino_nanoepoch()
#endif
                    ;

                bool is_exception = 
#ifdef __cplusplus >= 201703L
                    m_count != std::uncaught_exceptions()
#else
                    std::uncaught_exception()
#endif
                    ;
                if(is_exception)
                {
                    neutrino_context_panic(ne, m_stream_id, m_event_id);
                }
                else
                {
                    neutrino_context_leave(ne, m_stream_id, m_event_id);
                }
            }
        };
    }
}