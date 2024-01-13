#pragma once

#include <memory>
#include <string_view>

#include "neutrino_transport_shared_mem_buffer.hpp"
#include "neutrino_transport_shared_mem_events_v00.hpp"
#include "neutrino_transport_shared_mem_header_v00_linux.hpp"

namespace neutrino
{
    namespace transport
    {
        namespace shared_memory
        {
            typedef buffers_ring_t<v00_shared_header_t, v00_events_set_t> buffers_ring_v00_linux_t;
            std::unique_ptr<buffers_ring_v00_linux_t> init_v00_base_linux(const std::string_view& cfg);
        }
    }
}
