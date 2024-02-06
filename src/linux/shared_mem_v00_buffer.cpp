#include <chrono>
#include <memory>
#include <functional>

#include <stdlib.h>

#include <neutrino_errors.hpp>
#include <neutrino_shared_mem_v00_buffer.hpp>

namespace neutrino::transport::shared_memory {

mem_buf_v00_linux_t init_v00_buffers_ring(const std::string_view& cfg_view)
{
    const char* neutrino_consumer_env_name = "NEUTRINO_CONSUMER_FD";
    mem_buf_v00_linux_t ret;

    std::function<void(buffers_ring_v00_linux_t*)> buffer_deleter = [](buffers_ring_v00_linux_t*){};

    if( cfg_view.find("process=consumer") ) {
        // this is consumer process, allocate the memory
        ret.first.reset(new initializer_memfd_t(1000000, nullptr));

        buffer_deleter = [](buffers_ring_v00_linux_t* d) { 
            d->destroy_all();
            delete d;
        };

        // export mem fd as env variable
        char buf[200];
        snprintf(buf, sizeof(buf)-1,"%ui",ret.first->m_fd);
        ::setenv(neutrino_consumer_env_name, buf, 1);

    } else {
        // this is producer process, map existing memory
        char* cp_consumer_fd = getenv(neutrino_consumer_env_name);
        if(cp_consumer_fd == nullptr) {
            throw configure::missing_option(std::source_location::current(), neutrino_consumer_env_name);
        }
        char* endptr = nullptr;
        const auto consumer_fd = strtoul(cp_consumer_fd, &endptr, 10);
        if(*endptr != '\x0') {
            throw configure::missing_option(std::source_location::current(), "value must be numeric, env var NEUTRINO_CONSUMER_FD");
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