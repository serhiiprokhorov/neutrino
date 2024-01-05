#pragma once

#include "neutrino_transport.hpp"
#include "neutrino_transport_shared_mem_buffer.hpp"
#include "neutrino_transport_shared_mem_port.hpp"
#include "neutrino_transport_shared_mem_v00_header_linux.hpp"
#include "neutrino_transport_shared_mem_v00_events.hpp"

namespace neutrino
{
    namespace transport 
    {
        namespace shared_memory
        {
            typedef buffers_ring_t<
                v00_shared_header_t,
                v00_events_set_t
            > buffers_ring_v00_linux_t;

            typedef synchronized_port_t<buffers_ring_v00_linux_t> synchronized_port_v00_linux_t;

            typedef exclusive_port_t<buffers_ring_v00_linux_t> exclusive_port_v00_linux_t;

            typedef lock_free_port_t<buffers_ring_v00_linux_t> lock_free_port_v00_linux_t;
        }
    }

    typedef consumer_proxy_t<transport::shared_memory::synchronized_port_v00_linux_t> consumer_proxy_synchronized_port_shared_mem_v00_linux_t;
    typedef consumer_proxy_t<transport::shared_memory::exclusive_port_v00_linux_t> consumer_proxy_exclusive_port_shared_mem_v00_linux_t;
    typedef consumer_proxy_t<transport::shared_memory::lock_free_port_v00_linux_t> consumer_proxy_lock_free_port_shared_mem_v00_linux_t;
}
