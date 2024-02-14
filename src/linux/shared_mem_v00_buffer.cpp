#include <chrono>
#include <memory>
#include <functional>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <neutrino_errors.hpp>
#include <neutrino_shared_mem_v00_buffer.hpp>

namespace shared_fd
{
// https://man.archlinux.org/man/memfd_create.2.en
const char* neutrino_consumer_env_name = "NEUTRINO_CONSUMER_FD";
const char* neutrino_producer_options_name = "NEUTRINO_PRODUCER_OPTIONS";
void export_fd( unsigned int shared_fd, const char* producer_options) {
    char shared_fd_path[1000];
    snprintf(shared_fd_path, sizeof(shared_fd_path)-1, "/proc/%u/fd/%u", getpid(), shared_fd);
    ::setenv(neutrino_consumer_env_name, shared_fd_path, 1);

    ::setenv(neutrino_producer_options_name, producer_options, 1);
};

int import_fd() {
    char* shared_fd_path = getenv(neutrino_consumer_env_name);
    if(shared_fd_path) {
        throw neutrino::configure::missing_option(std::source_location::current(), neutrino_consumer_env_name);
    }
    int ret = open( shared_fd_path, O_RDWR);
    if(ret == -1) {
        throw neutrino::os::errno_error(std::source_location::current(), std::string("can't open shared file: ").append(shared_fd_path).c_str());
    }
    char* shared_fd_path = getenv(neutrino_producer_options_name);
    return ret;
};
}

namespace neutrino::transport::shared_memory {

namespace options
{
    const std::string consumer_buffer_total_size_kw("size=");
    const std::string common_buffer_number_kw("buffers=");
}

mem_buf_v00_linux_t init_v00_buffers_ring(const std::string_view& cfg_view)
{
    mem_buf_v00_linux_t ret;

    std::function<void(buffers_ring_v00_linux_t*)> buffer_initializer = [](buffers_ring_v00_linux_t*){};
    std::function<void(buffers_ring_v00_linux_t*)> buffer_deleter = [](buffers_ring_v00_linux_t*){};

    if( cfg_view.find("process=consumer") != std::string_view::npos) {
        const auto sz_offset = cfg_view.find(options::consumer_buffer_total_size_kw);

        if(sz_offset == std::string_view::npos) {
            throw configure::missing_option(std::source_location::current(), options::consumer_buffer_total_size_kw);
        }

        char* sz_endptr = nullptr;
        const auto sz = strtoul(cfg_view.data() + sz_offset + options::consumer_buffer_total_size_kw.size(), &sz_endptr, 10);
        if(*sz_endptr != ',') {
            throw configure::not_a_number_option_value(std::source_location::current(), options::consumer_buffer_total_size_kw);
        }

        if(sz < 10 || sz > 1000000000) {
            throw configure::impossible_option_value(std::source_location::current(), options::consumer_buffer_total_size_kw);
        }

        // this is consumer process, allocate the memory
        ret.first.reset(new initializer_memfd_t(sz, nullptr, shared_fd::export_fd));

        buffer_deleter = [](buffers_ring_v00_linux_t* d) { 
            d->destroy_all();
            delete d;
        };
        buffer_initializer = [](buffers_ring_v00_linux_t*d){
            d->init_all();
        };

    } else {
        // this is producer process, map existing memory
        ret.first.reset(new initializer_memfd_t(shared_fd::import_fd()));

        buffer_deleter = [](buffers_ring_v00_linux_t* d) { 
            delete d;
        };
    }

    // TODO: get number of buffers from the config
    const auto cc_buffers_offset = cfg_view.find(options::common_buffer_number_kw);

    if(cc_buffers_offset == std::string_view::npos) {
        throw configure::missing_option(std::source_location::current(), options::common_buffer_number_kw);
    }

    char* cc_buffers_sz_endptr = nullptr;
    const std::size_t cc_buffers = strtoul(cfg_view.data() + cc_buffers_offset + options::common_buffer_number_kw.size(), &cc_buffers_sz_endptr, 10);
    if(*cc_buffers_sz_endptr != ',') {
        throw configure::not_a_number_option_value(std::source_location::current(), options::common_buffer_number_kw);
    }

    std::shared_ptr<buffers_ring_v00_linux_t> my_ret(
        new buffers_ring_v00_linux_t(ret.first->data(), ret.first->size(), cc_buffers),
        buffer_deleter
    );


    ret.second.swap(
        my_ret
    );

    buffer_initializer(ret.second.get());

    return ret;
}
}