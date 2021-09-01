#pragma once

#include <memory>
#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            std::shared_ptr<endpoint_proxy_t> create_endpoint_proxy(const char* cfg, const uint32_t cfg_bytes);
        }
    }
}