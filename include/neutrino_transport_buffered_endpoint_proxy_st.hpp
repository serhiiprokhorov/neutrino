#pragma once

#include "neutrino_transport_buffered_endpoint_proxy.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct singlethread_shared_memory_endpoint_proxy_t : public shared_memory_endpoint_proxy_t
            {
                using shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_t;

                bool consume(const uint8_t* p, const uint8_t* e) override;
            };
        }
    }
}
