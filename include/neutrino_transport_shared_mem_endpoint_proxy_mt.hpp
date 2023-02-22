#pragma once

#include <mutex>
#include <atomic>
#include "neutrino_transport_shared_mem_endpoint_proxy_st.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct exclusive_mt_shared_memory_endpoint_proxy_t : public singlethread_shared_memory_endpoint_proxy_t
            {
                using singlethread_shared_memory_endpoint_proxy_t::singlethread_shared_memory_endpoint_proxy_t;

                std::mutex m_buffer_mtx;

                bool consume(const uint8_t* p, const uint8_t* e) override;
            };

            struct optimistic_mt_shared_memory_endpoint_proxy_t : public shared_memory_endpoint_proxy_t
            {
                struct optimistic_mt_shared_memory_endpoint_proxy_params_t
                {
                    std::size_t m_optimistic_lock_retries{ 1000 };
                } const m_params;

                std::atomic_uint64_t m_dirty_count = 0;

                optimistic_mt_shared_memory_endpoint_proxy_t(
                    const shared_memory_endpoint_proxy_params_t& po
                    , std::shared_ptr<shared_memory::pool_t> pool
                    , const optimistic_mt_shared_memory_endpoint_proxy_params_t opo
                )
                    : m_params(opo), shared_memory_endpoint_proxy_t(po, pool)
                {
                }

                bool consume(const uint8_t* p, const uint8_t* e) final;
            };

        }
    }
}
