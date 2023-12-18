#pragma once

#include "neutrino_transport_shared_mem.hpp"

namespace neutrino
{
    namespace transport
    {
        namespace shared_memory
        {
            /// @brief consumer proxy for singlethread environment; not thread safe
            struct singlethread_ring_consumer_proxy_t : public ring_consumer_proxy_t
            {
                using ring_consumer_proxy_t::ring_consumer_proxy_t;
                uint64_t m_dirty_buffer_counter{1};

                bool schedule_for_transport(const uint8_t* b, const uint8_t* e) override;
            };
        }
    }
}
