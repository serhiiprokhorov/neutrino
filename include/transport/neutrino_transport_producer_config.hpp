#pragma once

#include <memory>
#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace transport
    {
        std::unique_ptr<consumer_proxy_t> create_consumer_proxy(const char* cfg, const uint32_t cfg_bytes);
    }
}