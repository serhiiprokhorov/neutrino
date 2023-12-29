#include <chrono>
#include <memory>

#include <neutrino_transport.hpp>
#include <neutrino_transport_shared_mem_v00_linux.hpp>

using namespace neutrino;
using namespace neutrino::transport::shared_memory;

typedef br_t buffers_ring_t<v00_buffer_layout_t, v00_events_set_t>;

struct consumer_control_t {
    std::unique_ptr<linux::initializer_memfd_t> m_memory;
    std::unique_ptr<br_t> m_ring;
    std::unique_ptr<consumer_t> m_consumer;
}

static std::unique_ptr<consumer_control_t> consumer_control;

extern "C"
{

void neutrino_producer_startup(const char* cfg, const uint32_t cfg_bytes)
{
    std::unique_ptr<consumer_control_t> my_consumer_control(new consumer_control_t);

    my_consumer_control->m_memory.reset(new linux::initializer_memfd_t());

    my_consumer_control->m_ring.reset(
        new br_t(
            *my_consumer_control->m_memory,
            false /* producer assumes the memory is already provided by consumer process */
        )
        );

    my_consumer_control->m_consumer.reset(
        new consumer_proxy_t<synchronized_producer_t<br_t>>(
            std::unique_ptr<synchronized_producer_t<br_t>>(
                new synchronized_producer_t<br_t>(*my_consumer_control->m_ring))
            );
    );

    consumer_control.swap(my_consumer_control);
}

void neutrino_producer_shutdown(void)
{
    consumer_control.reset(new consumer_t());
}

neutrino_nanoepoch_t neutrino_nanoepoch(void)
{
    return neutrino_nanoepoch_t{ .v = 
        uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
    };
}

void neutrino_checkpoint(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    consumer_control->m_consumer->consume_checkpoint(
        nanoepoch, stream_id, event_id
    );
}

void neutrino_context_enter(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    consumer_control->m_consumer->consume_context_enter(
        nanoepoch, stream_id, event_id
    );
}

void neutrino_context_leave(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    consumer_control->m_consumer->consume_context_leave(
        nanoepoch, stream_id, event_id
    );
}

void neutrino_context_exception(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    consumer_control->m_consumer->consume_context_exception(
        nanoepoch, stream_id, event_id
    );
}

void neutrino_flush()
{
    //transport::get_consumer_proxy().m_endpoint.flush();
}

} // extern "C"
