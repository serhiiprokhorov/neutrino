#include <chrono>
#include <memory>
#include <stdlib.h>
#include <iostream>

#include <optional>

#include <string_view>

#include <neutrino_consumer.hpp>
#include <neutrino_errors.hpp>

#include <../neutrino_config.hpp>

namespace neutrino::consumer::configure
{

void from_cmd_line_args(int argc, char* argv[])
{
    std::optional<std::string> transport;
    std::optional<std::size_t> shm_size;
    std::optional<std::size_t> sync_bytes;
    std::optional<std::string> sync_mode;

    std::function<void(const char*)> next_arg_default = [](const char*) {};
    std::function<void(const char*)> next_arg_op = [&](const char* arg) {
        if(arg == "--transport") {
            return [&](const char* arg) { 
                transport.emplace(ensure_string(arg)); return next_arg_default; 
            };
        }
        if(arg == "--shm_size") {
            return [&](const char* arg) { 
                shm_size.emplace(ensure_number<std::size_t>(arg)); return next_arg_default; 
            };
        }
        if(arg == "--sync_bytes") {
            return [&](const char* arg) { 
                sync_bytes.emplace(ensure_number<std::size_t>(arg)); return next_arg_default; 
            };
        }
        if(arg == "--sync_mode") {
            return [&](const char* arg) { 
                sync_mode.emplace(arg); return next_arg_default; 
            };
        }
        throw std::runtime_error(std::string("unknown option: ").append(arg));
    };
    next_arg_default = next_arg_op;

    for(int cc = 1; cc < argc; cc++) {
        if(argv[cc] == nullptr)
            break;
        next_arg_op = next_arg_op(argv[cc]);
    }

    if(!transport) {
        throw std::runtime_error("missing --transport");
    }

    if(int err = setenv(env_NEUTRINO_PRODUCER_TRANSPORT, std::string(transport), 1))
        throw std::runtime_error("can't set env var");

    if(transport == opt_transport_shared_mem) {
        if(!sync_mode) {
            throw std::runtime_error("missing --sync_mode");
        }
        if(!shm_size) {
            throw std::runtime_error("missing --shm_size");
        }
        if(!sync_bytes) {
            throw std::runtime_error("missing --sync_bytes");
        }

        if(int err = setenv(env_NEUTRINO_PRODUCER_SYNC_MODE, std::string(sync_mode), 1))
            throw std::runtime_error("can't set env var");

        if(int err = setenv(env_NEUTRINO_PRODUCER_SHM_SIZE, std::string(shm_size), 1))
            throw std::runtime_error("can't set env var");

        if(int err = setenv(env_NEUTRINO_PRODUCER_SYNC_BYTES, std::string(sync_bytes), 1))
            throw std::runtime_error("can't set env var");

        const auto init_buffer = [&shm_size]() {
            return neutrino::transport::shared_memory::initializer_memfd_t(*shm_size, const char*, std::function<void(unsigned int)>);
        };

        // if(sync_mode == opt_transport_shared_mem_sync_mode_exclusive)
        //     neutrino::producer::configure::shared_mem_v00_exclusive_linux(init_buffer(), shared_cfg.sync_bytes);
        // } else
        // if(sync_mode == opt_transport_shared_mem_sync_mode_lockfree) {
        //     neutrino::producer::configure::shared_mem_v00_lockfree_linux(init_buffer(), shared_cfg.sync_bytes);
        // } else
        // if(sync_mode == opt_transport_shared_mem_sync_mode_synchronized) {
        //     neutrino::producer::configure::shared_mem_v00_synchronized_linux(init_buffer(), shared_cfg.sync_bytes);
        // } else
        //     throw std::runtime_error("--sync_mode unknown value");
    } else {
        throw std::runtime_error("--transport unknown value");
    }
}
}