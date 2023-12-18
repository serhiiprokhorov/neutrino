#pragma once

#include <mutex>
#include <atomic>
#include "neutrino_transport_shared_mem_endpoint_proxy_st.hpp"

namespace neutrino
{
        namespace transport
        {
            namespace shared_memory
            {
                /// @brief consumer proxy for multi thread environment which provides exclusive access to a transport 
                struct exclusive_mt_ring_consumer_proxy_t : public singlethread_shared_memory_consumer_proxy_t
                {
                    using singlethread_shared_memory_consumer_proxy_t::singlethread_shared_memory_consumer_proxy_t;

                    std::mutex m_buffer_mtx;

                    bool schedule_for_transport(const uint8_t* b, const uint8_t* e) override;
                };

                /// @brief consumer proxy for multi thread environment which provides "lock-free" concurrent access to a transport 
                struct optimistic_mt_ring_consumer_proxy_t : public ring_consumer_proxy_t
                {
                    struct params_t
                    {
                        std::size_t m_optimistic_lock_retries{ 1000 };
                    } const m_params;

                    std::atomic_uint64_t m_dirty_count = 0;

                    optimistic_mt_ring_consumer_proxy_t(
                        const ring_consumer_proxy_t::params_t& po
                        , std::shared_ptr<shared_memory::shared_buffers_ring_t> ring
                        , const params_t opo
                    )
                        : m_params(opo), ring_consumer_proxy_t(po, ring)
                    {
                    }

                    bool schedule_for_transport(const uint8_t* b, const uint8_t* e) override;
                };
            }
        }
}
