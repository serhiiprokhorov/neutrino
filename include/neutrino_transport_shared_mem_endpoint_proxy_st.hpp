#pragma once

#include "neutrino_transport_shared_mem.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct singlethread_shared_memory_endpoint_proxy_t : public shared_memory_endpoint_proxy_t
            {
                using shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_t;
                std::atomic<uint64_t> m_dirty_buffer_counter{1};

                bool consume(const uint8_t* p, const uint8_t* e) override;
            };
        }
    }
}
