#include <chrono>
#include <memory>
#include <stdlib.h>

#include <string_view>

#include <neutrino_transport_shared_mem_v00_events.hpp>
#include <neutrino_transport_shared_mem_v00_header_linux.hpp>
#include <neutrino_shared_mem_initializer_linux.hpp>

#include <neutrino_transport_shared_mem_buffer.hpp>
#include <neutrino_transport_shared_mem_port.hpp>

#include <neutrino_transport.hpp>

namespace
{

static std::unique_ptr<lockfree_port_t<buffers_ring_v00_linux_t>> port;

static void neutrino_producer_shutdown_impl(void)
{
    port.reset();
}

static void neutrino_checkpoint_impl(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    port->put<v00_events_set_t::event_checkpoint_t>(nanoepoch, stream_id, event_id)
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
    port->flush();
}

}

namespace neutrino::producer::configure
{
void shared_mem_v00_lockfree_linux(const std::u8string_view& cfg_view)
{
    port.reset(new lockfree_port_t<buffers_ring_v00_linux_t>>(transport::shared_memory::init_v00_base_linux(cfg_view));

    ::neutrino_producer_shutdown = neutrino_producer_shutdown_impl;
    ::neutrino_produce_checkpoint = neutrino_produce_checkpoint_impl;
    ::neutrino_produce_context_enter = neutrino_produce_context_enter_impl;
    ::neutrino_produce_context_leave = neutrino_produce_context_leave_impl;
    ::neutrino_produce_context_exception = neutrino_produce_context_exception_impl;
    ::neutrino_producer_flush = neutrino_producer_flush_impl;
}
}