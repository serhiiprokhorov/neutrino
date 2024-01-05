#include <chrono>
#include <memory>
#include <stdlib.h>

#include <neutrino_shared_mem_linux.hpp>
#include <neutrino_transport_shared_mem_v00_linux.hpp>

using namespace neutrino::transport::shared_memory;

struct consumer_control_v00_linux_t {
    std::unique_ptr<initializer_memfd_t> m_memory;
    std::unique_ptr<buffers_ring_v00_linux_t> m_ring;
    std::unique_ptr<consumer_t> m_consumer;
};

static std::unique_ptr<consumer_control_t> consumer_control;

extern "C"
{

void neutrino_producer_startup(const char* cfg, const uint32_t cfg_bytes)
{
    std::unique_ptr<consumer_control_t> my_consumer_control(new consumer_control_t);

    my_consumer_control->m_memory.reset(new initializer_memfd_t());

    const std::size_t cc_buffers = 5;
    my_consumer_control->m_ring.reset(
        new buffers_ring_v00_linux_t(
            *my_consumer_control->m_memory,
            cc_buffers
        )
        );

    if(strncmp(cfg, "synchronized", cfg_bytes) == 0) {
        my_consumer_control.m_consumer.reset(
            new consumer_proxy_synchronized_port_shared_mem_v00_linux_t(
                std::make_unique(new synchronized_port_v00_linux_t(*ret.m_ring))
            ));
    }
    else if(strncmp(cfg, "exclusive", cfg_bytes) == 0) {
        my_consumer_control.m_consumer.reset(
            new consumer_proxy_exclusive_port_shared_mem_v00_linux_t(
                std::make_unique(new exclusive_port_v00_linux_t(*ret.m_ring))
            ));
    }
    else if(strncmp(cfg, "lock_free", cfg_bytes) == 0) {
        my_consumer_control.m_consumer.reset(
            new consumer_proxy_lock_free_port_shared_mem_v00_linux_t(
                std::make_unique(new lock_free_port_v00_linux_t(*ret.m_ring, 1))
            ));
    }

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
