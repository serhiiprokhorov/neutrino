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

using namespace neutrino::transport::shared_memory;

typedef buffers_ring_t< v00_shared_header_t, v00_events_set_t> buffers_ring_v00_linux_t;

typedef synchronized_port_t<buffers_ring_v00_linux_t> synchronized_port_v00_linux_t;
typedef exclusive_port_t<buffers_ring_v00_linux_t> exclusive_port_v00_linux_t;
typedef lock_free_port_t<buffers_ring_v00_linux_t> lock_free_port_v00_linux_t;

struct consumer_control_v00_linux_t {
    std::unique_ptr<initializer_memfd_t> m_memory;
    std::unique_ptr<buffers_ring_v00_linux_t> m_ring;
    std::unique_ptr<consumer_t> m_consumer;

    consumer_control_v00_linux_t() = default;
    ~consumer_control_v00_linux_t() = default;
};

static std::unique_ptr<consumer_control_v00_linux_t> consumer_control;

extern "C"
{

void neutrino_producer_startup(const char* cfg, const uint32_t cfg_bytes)
{
    const std::u8string_view cfg_view(cfg, cfg_bytes);

    std::unique_ptr<consumer_control_v00_linux_t> my_consumer_control(new consumer_control_v00_linux_t);

    if(cfg_view.find("consumer,") != std::string_view::npos) {
        // TODO: get bytes from config
        my_consumer_control->m_memory.reset(new initializer_memfd_t(1000000, nullptr));
    } else {
        char* cp_consumer_fd = getenv("NEUTRINO_CONSUMER_FD");
        if(cp_consumer_fd == nullptr) {
            // TODO: report error
            return;
        }
        char* endptr = nullptr;
        const auto consumer_fd = strtoul(cp_consumer_fd, &endptr, 10);
        if(*endptr != '\x0') {
            // TODO: report error
            return;
        }
        my_consumer_control->m_memory.reset(new initializer_memfd_t(consumer_fd));
    }

    // TODO: get number of buffers from the config
    const std::size_t cc_buffers = 5;
    my_consumer_control->m_ring.reset(
        new buffers_ring_v00_linux_t(
            *(my_consumer_control->m_memory),
            cc_buffers
        )
        );

    if(cfg_view.find("synchronized,") != std::string_view) {
        my_consumer_control.m_consumer.reset(
            new consumer_proxy_t<synchronized_port_v00_linux_t>(
                std::make_unique(new synchronized_port_v00_linux_t(*ret.m_ring))
            )
        );
    }
    else if(strncmp(cfg, "exclusive", cfg_bytes) == 0) {
        my_consumer_control.m_consumer.reset(
            new consumer_proxy_t<exclusive_port_v00_linux_t>(
                std::make_unique(new exclusive_port_v00_linux_t(*ret.m_ring))
            )
        );
    }
    else if(strncmp(cfg, "lock_free", cfg_bytes) == 0) {
        // TODO: get retries from the config
        const uint64_t cc_retries_serialize = 10;
        my_consumer_control.m_consumer.reset(
            new consumer_proxy_t<lock_free_port_v00_linux_t>(
                std::make_unique(new lock_free_port_v00_linux_t(*ret.m_ring, cc_retries_serialize))
            )
        );
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
