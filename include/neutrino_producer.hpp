#pragma once

#include <stdexcept>
#include <memory>
#include "neutrino_producer.h"
#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace producer
        {
            struct config_t
            {
                std::string m_producer_id{ 0 }; // TODO: UUID?
                std::string m_producer_role;
                /*
                std::unique_ptr<transport::endpoint_consumer_t::endpoint_params_t> ep;
                std::unique_ptr<transport::async_posix_consumer_t::async_posix_consumer_params_t> aep;

                std::unique_ptr < transport::buffered_consumer_t::buffered_consumer_params_t> bpo;
                std::unique_ptr < transport::buffered_exclusive_consumer_t::buffered_exclusive_consumer_params_t> epo;
                std::unique_ptr < transport::buffered_optimistic_consumer_t::buffered_optimistic_consumer_params_t> opo;
                */
            };

            const config_t configure_from_json(const char* cfg, const uint32_t cfg_bytes);

            std::shared_ptr<transport::consumer_stub_t> set_consumer(std::shared_ptr<transport::consumer_stub_t> p) noexcept;
            std::shared_ptr<transport::consumer_stub_t> get_consumer() noexcept;

        }
    }

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