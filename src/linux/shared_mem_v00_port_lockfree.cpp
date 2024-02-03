#include <chrono>
#include <memory>
#include <stdlib.h>

#include <string_view>

#include <neutrino_shared_mem_v00_port_lockfree.hpp>

namespace
{

using namespace neutrino::transport::shared_memory;

struct port_ex_t : public lock_free_port_t<buffers_ring_v00_linux_t> {
    std::pair<
        std::shared_ptr<initializer_memfd_t>,
        std::shared_ptr<buffers_ring_v00_linux_t>
    > m_mem_buf;
    port_ex_t(std::pair<std::shared_ptr<initializer_memfd_t>,std::shared_ptr<buffers_ring_v00_linux_t>> mem_buf, const uint64_t lock_free_reps)
        : m_mem_buf(mem_buf), lock_free_port_t<buffers_ring_v00_linux_t>(mem_buf.second->get_first(), lock_free_reps) {}
};

static std::unique_ptr<port_ex_t> port;

static void neutrino_producer_shutdown_impl(void)
{
    port.reset();
}

static void neutrino_checkpoint_impl(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    port->put<v00_events_set_t::event_checkpoint_t>(nanoepoch, stream_id, event_id);
}

static void neutrino_context_enter_impl(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    port->put<v00_events_set_t::event_context_enter_t>(nanoepoch, stream_id, event_id);
}

static void neutrino_context_leave_impl(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    port->put<v00_events_set_t::event_context_leave_t>(nanoepoch, stream_id, event_id);
}

static void neutrino_context_exception_impl(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    port->put<v00_events_set_t::event_context_exception_t>(nanoepoch, stream_id, event_id);
}

static void neutrino_flush_impl()
{
    //port->flush();
}

}

namespace neutrino::producer::configure
{
void shared_mem_v00_lockfree_linux(const std::string_view& cfg_view)
{
    // TODO: from cfg
    const uint64_t lock_free_reps = 100;
    port = std::make_unique<port_ex_t>(neutrino::transport::shared_memory::init_v00_buffers_ring(cfg_view), lock_free_reps);

    ::neutrino_producer_shutdown = neutrino_producer_shutdown_impl;
    ::neutrino_produce_checkpoint = neutrino_checkpoint_impl;
    ::neutrino_produce_context_enter = neutrino_context_enter_impl;
    ::neutrino_produce_context_leave = neutrino_context_leave_impl;
    ::neutrino_produce_context_exception = neutrino_context_exception_impl;
    ::neutrino_producer_flush = neutrino_flush_impl;
}
}