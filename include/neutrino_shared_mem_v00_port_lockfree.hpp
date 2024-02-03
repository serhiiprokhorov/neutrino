#pragma once

#include <memory>

#include <neutrino_shared_mem_initializer_linux.hpp>

#include <neutrino_shared_mem_v00_header.hpp>
#include <neutrino_shared_mem_v00_buffer.hpp>

#include <neutrino_transport_shared_mem_v00_events.hpp>
#include <neutrino_transport_shared_mem_buffer.hpp>
#include <neutrino_transport_shared_mem_port.hpp>

#include <neutrino_transport.hpp>

#include <neutrino_producer.h>

namespace neutrino::transport::shared_memory
{

struct lock_free_port_v00_linux_t : public lock_free_port_t<buffers_ring_v00_linux_t> {
    std::pair<
        std::shared_ptr<initializer_memfd_t>,
        std::shared_ptr<buffers_ring_v00_linux_t>
    > m_mem_buf;
    lock_free_port_v00_linux_t(std::pair<std::shared_ptr<initializer_memfd_t>,std::shared_ptr<buffers_ring_v00_linux_t>> mem_buf, const uint64_t lock_free_reps)
        : m_mem_buf(mem_buf), lock_free_port_t<buffers_ring_v00_linux_t>(mem_buf.second->get_first(), lock_free_reps) {}
};

}