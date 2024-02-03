#include <chrono>
#include <memory>
#include <functional>

#include <stdlib.h>

#include <neutrino_errors.hpp>
#include <neutrino_shared_mem_v00_buffer.hpp>

namespace neutrino::transport::shared_memory {

mem_buf_v00_linux_t init_v00_buffers_ring(const std::string_view& cfg_view)
{
    mem_buf_v00_linux_t ret;

    std::function<void(buffers_ring_v00_linux_t*)> buffer_deleter = [](buffers_ring_v00_linux_t*){};

    if( cfg_view.find("process=consumer") ) {
        // this is consumer process, allocate the memory
        ret.first.reset(new initializer_memfd_t(1000000, nullptr));

        buffer_deleter = [](buffers_ring_v00_linux_t* d) { 
            d->destroy_all();
            delete d;
        };
    } else {
        // this is producer process, map existing memory
        char* cp_consumer_fd = getenv("NEUTRINO_CONSUMER_FD");
        if(cp_consumer_fd == nullptr) {
            throw configure::missing_option("env var NEUTRINO_CONSUMER_FD");
        }
        char* endptr = nullptr;
        const auto consumer_fd = strtoul(cp_consumer_fd, &endptr, 10);
        if(*endptr != '\x0') {
            throw configure::missing_option("value must be numeric, env var NEUTRINO_CONSUMER_FD");
        }
        ret.first.reset(new initializer_memfd_t(consumer_fd));

        buffer_deleter = [](buffers_ring_v00_linux_t* d) { 
            delete d;
        };
    }

    // TODO: get number of buffers from the config
    const std::size_t cc_buffers = 5;

    std::shared_ptr<buffers_ring_v00_linux_t> my_ret(
        new buffers_ring_v00_linux_t(ret.first->data(), ret.first->size(), cc_buffers),
        buffer_deleter
    );

    ret.second.swap(my_ret);

    return ret;
}
}