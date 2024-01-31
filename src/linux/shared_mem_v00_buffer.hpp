#pragma once

#include <memory>
#include <utility>
#include <string_view>

#include <neutrino_shared_mem_initializer_linux.hpp>
#include <neutrino_transport_shared_mem_buffer.hpp>
#include <neutrino_transport_shared_mem_v00_events.hpp>
#include "shared_mem_v00_header.hpp"

namespace neutrino
{
    namespace transport
    {
        namespace shared_memory
        {
            typedef buffers_ring_t<buffer_t<v00_shared_header_t, v00_events_set_t>> buffers_ring_v00_linux_t;
            typedef std::pair<
                std::shared_ptr<initializer_memfd_t>,
                std::shared_ptr<buffers_ring_v00_linux_t>
            > mem_buf_v00_linux_t;

            mem_buf_v00_linux_t init_v00_buffers_ring(const std::string_view& cfg);
        }
    }
}
