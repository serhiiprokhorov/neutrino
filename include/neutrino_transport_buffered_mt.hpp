#pragma once

#include <mutex>
#include <atomic>
#include "neutrino_transport_buffered_st.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct buffered_exclusive_endpoint_t : public buffered_singlethread_endpoint_t
            {
                using buffered_singlethread_endpoint_t::buffered_singlethread_endpoint_t;

                std::mutex m_buffer_mtx;

                std::ptrdiff_t consume(const uint8_t* p, const uint8_t* e) override;
            };

            struct buffered_optimistic_endpoint_t : public buffered_endpoint_t
            {
                struct buffered_optimistic_consumer_params_t
                {
                    std::size_t m_optimistic_lock_retries{ 1000 };
                } const m_params;

                std::atomic<uint64_t> m_frame_start{ 0 };

                buffered_optimistic_endpoint_t(
                    std::shared_ptr<endpoint_t> endpoint
                    , const buffered_endpoint_t::buffered_endpoint_params_t bpo
                    , const buffered_optimistic_consumer_params_t po
                )
                    : m_params(po), buffered_endpoint_t(endpoint, bpo)
                {
                }

                std::ptrdiff_t consume(const uint8_t* p, const uint8_t* e) final;
            protected:
                bool flush() final;

            };

        }
    }
}
