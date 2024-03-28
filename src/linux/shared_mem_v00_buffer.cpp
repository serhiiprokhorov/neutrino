#include <chrono>
#include <memory>
#include <functional>
#include <charconv>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <neutrino_errors.hpp>
#include <neutrino_shared_mem_v00_buffer.hpp>

#include "shared_mem_config.cpp"



// this namespace implements case when consumer and producer exchange shared mem file via env variable
namespace shared_fd
{
    namespace env_vars {
        const char* consumer_fd_path = "NEUTRINO_CONSUMER_FD_PATH";
        const char* producer_options = "NEUTRINO_PRODUCER_OPTIONS";
    }

    void export_fd( unsigned int shared_fd, const char* desired_producer_options) {
        char shared_fd_path[1000];
        snprintf(shared_fd_path, sizeof(shared_fd_path)-1, "/proc/%u/fd/%u", getpid(), shared_fd);
        ::setenv(env_vars::consumer_fd_path, shared_fd_path, 1);

        ::setenv(env_vars::producer_options, desired_producer_options, 1);
    };

    std::pair<int, std::string> import_fd() {
        char* shared_fd_path = getenv(env_vars::consumer_fd_path);
        if(!shared_fd_path) {
            throw neutrino::configure::missing_option(std::source_location::current(), env_vars::consumer_fd_path);
        }
        int ret = open( shared_fd_path, O_RDWR);
        if(ret == -1) {
            throw neutrino::os::errno_error(std::source_location::current(), std::string("can't open shared file: ").append(shared_fd_path).c_str());
        }
        char* desired_producer_options = getenv(env_vars::producer_options);
        if(!desired_producer_options) {
            throw neutrino::configure::missing_option(std::source_location::current(), env_vars::producer_options);
        }
        return {ret, (const char*)desired_producer_options};
    };
}

namespace neutrino::transport::shared_memory {

namespace options
{
    struct values {
        bool m_is_consumer = false;
        std::size_t m_shm_size = 0;
        std::size_t m_cc_buffer = 0;

        const config_value_t& ensure_value(const config_value_t& cv) const {
            if(cv.m_value.empty())
                throw neutrino::configure::missing_option(std::source_location::current(), cv.m_name);
            return cv.m_value;
        }

        const unsigned long ensure_value_ul(const config_value_t& cv) const {
            unsigned long ret = 0;
            auto [ptr, ec] = from_chars( cv.m_value.data(), cv.m_value.data() + cv.m_value.size(), ret);

            if(ec == std::errc()) {
                return ret;
            }
            if (ec == std::errc::invalid_argument)
                // not a number
                throw neutrino::configure::impossible_option_value(std::source_location::current(), cv.m_name);
            if (ec == std::errc::result_out_of_range)
                // value too big
                throw neutrino::configure::impossible_option_value(std::source_location::current(), cv.m_name);
        }

        values& set_consumer(const config_value_t& cv) {

            if(cv.m_value.compare("consumer") )
            {
                m_is_consumer = true;
            }
            else
            if(cv.m_value.compare("producer") )
            {
                m_is_consumer = false;
            }
            else
            {
                throw neutrino::configure::impossible_option_value(std::source_location::current(), cv.m_name);
            }
            return *this;
        }

        values& set_shm_size(const config_value_t& cv) {
            return *this;
        }

        values& set_cc_buffer(const config_value_t& cv) {
            return *this;
        }

        values(const config_set_t& s) {

            set_consumer(s.get_process_kind().ensure_value());
            set_shm_size(s.get_shm_size().ensure_value());
            set_cc_buffer(s.get_cc_buffers().ensure_value()); 

            const auto& cv_cc_buffers = s.get_cc_buffers();
            ) {
                m_cc_buffer;
            } else {
                throw neutrino::configure::missing_option(std::source_location::current(), env_vars::consumer_fd_path);
            }
            if(auto* cv = s.get_process_kind()) {
                m_is_consumer;
            } else {
                throw neutrino::configure::missing_option(std::source_location::current(), env_vars::consumer_fd_path);
            }
            if(auto* cv = s.get_process_kind()) {
                m_shm_size;
            } else {
                throw neutrino::configure::missing_option(std::source_location::current(), env_vars::consumer_fd_path);
            }
        }
    };
}

mem_buf_v00_linux_t init_v00_buffers_ring(const std::string_view& cfg_view)
{
    const auto ret = config_set_from_comma_separated(cfg_view);

    if(!ret.first) {
        throw configure::missing_option(std::source_location::current(), options::consumer_buffer_total_size_kw);
    }


    mem_buf_v00_linux_t ret;

    // buffer initializer logic:
    // - consumer: initializes a ring of buffers over the shared memory
    // - producer: interprets shared memory as a ring of buffers, no initialization is done
    std::function<void(buffers_ring_v00_linux_t*)> buffer_initializer = [](buffers_ring_v00_linux_t*){};
    // buffer deleter logic:
    // - consumer: deletes previously initialized ring of buffers over the shared memory
    // - producer: does nothing
    std::function<void(buffers_ring_v00_linux_t*)> buffer_deleter = [](buffers_ring_v00_linux_t*){};

    if( cfg_view.find("process=consumer,") != std::string_view::npos) {
        const auto sz_offset = cfg_view.find(options::shm_total_size_kw);

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