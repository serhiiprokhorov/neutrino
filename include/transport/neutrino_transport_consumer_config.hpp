#pragma once

#include <memory>
#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace transport
    {
        std::shared_ptr<consumer_t> create_endpoint(const char* cfg, const uint32_t cfg_bytes);
    }
}