#pragma once

/** Neutrino producer API */

#include "neutrino_types.h"

#ifdef __cplusplus
extern "C"
{
#endif
    /* C API : enables C lang integration by duplicating consumer interface */

    void neutrino_producer_startup(const char* cfg, const uint32_t cfg_bytes);
    void neutrino_producer_shutdown(void);
    void neutrino_producer_flush();

    /*TODO: errors as retvals?*/

    neutrino_nanoepoch_t neutrino_gen_nanoepoch(void);
    
    void neutrino_produce_checkpoint(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        );
    void neutrino_produce_context_enter(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        );
    void neutrino_produce_context_leave(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        );
    void neutrino_produce_context_exception(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        );

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <exception>

/* C++ extensions */
namespace neutrino
{
    namespace helpers
    {
        struct context_t
        {
            neutrino_event_id_t m_event_id;
            neutrino_stream_id_t m_stream_id;
            
            int m_ex_count_at_ctx_entry = std::uncaught_exceptions();

            context_t(
                neutrino_stream_id_t stream_id
                , neutrino_event_id_t event_id
            )
                : m_event_id(event_id), m_stream_id(stream_id)
            {
                neutrino_produce_context_enter(
                    neutrino_gen_nanoepoch()
                    , stream_id, event_id);
            }
            ~context_t()
            {
                const auto ne = neutrino_gen_nanoepoch();

                const bool is_exception = 
                    m_ex_count_at_ctx_entry != std::uncaught_exceptions();

                if(is_exception)
                {
                    neutrino_produce_context_exception(ne, m_stream_id, m_event_id);
                }
                else
                {
                    neutrino_produce_context_leave(ne, m_stream_id, m_event_id);
                }
            }
        };
    }
}
#endif
