#include <chrono>
#include <memory>
#include <stdlib.h>

#include <neutrino_transport_shared_mem_buffer_v00_linux.hpp>

namespace neutrino::transport::shared_memory {

std::unique_ptr<buffers_ring_v00_linux_t> init_v00_base_linux(const std::string_view& cfg)
{
    const std::u8string_view cfg_view(cfg, cfg_bytes);

    auto memory_initializer = []() -> std::unique_ptr<initializer_memfd_t> { throw std::runtime_error("process=... is not supported"); }

    std::unique_ptr<initializer_memfd_t> memory;

    if( cfg_view.find("process=consumer") ) {
        // consumer process
        memory_initializer = []() { return std::unique_ptr<initializer_memfd_t>(new initializer_memfd_t(1000000, nullptr)); }
    } else {
        // producer process
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
        memory_initializer = []() { return std::unique_ptr<initializer_memfd_t>(new initializer_memfd_t(consumer_fd)); }
    }

    // TODO: get number of buffers from the config
    const std::size_t cc_buffers = 5;

    return std::unique_ptr<buffers_ring_v00_linux_t>(
        new buffers_ring_v00_linux_t(
            memory_initializer(),
            cc_buffers
        )
    );
}
}