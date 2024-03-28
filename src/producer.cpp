#include <chrono>
#include <memory>
#include <stdlib.h>
#include <iostream>

#include <optional>

#include <string_view>

#include <neutrino_producer.h>
#include <neutrino_errors.hpp>

#include <../neutrino_config.hpp>


extern "C"
{

void (*neutrino_producer_shutdown)(void) = [](){};

void (*neutrino_produce_checkpoint)(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        ) = [](const neutrino_nanoepoch_t,const neutrino_stream_id_t,const neutrino_event_id_t){};

void (*neutrino_produce_context_enter)(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        ) = [](const neutrino_nanoepoch_t,const neutrino_stream_id_t,const neutrino_event_id_t){};

void (*neutrino_produce_context_leave)(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        ) = [](const neutrino_nanoepoch_t,const neutrino_stream_id_t,const neutrino_event_id_t){};

void (*neutrino_produce_context_exception)(
        const neutrino_nanoepoch_t nanoepoch, 
        const neutrino_stream_id_t stream_id, 
        const neutrino_event_id_t event_id
        ) = [](const neutrino_nanoepoch_t,const neutrino_stream_id_t,const neutrino_event_id_t){};

void (*neutrino_producer_flush)() = [](){};

neutrino_nanoepoch_t neutrino_nanoepoch_impl(void)
{
    return neutrino_nanoepoch_t{ .v = 
        uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
    };
}

neutrino_nanoepoch_t (*neutrino_nanoepoch)(void) = neutrino_nanoepoch_impl;

void neutrino_producer_startup(const char* cfg, const uint32_t cfg_bytes)
{
    neutrino::configure::producer::from_env_variables();
}

} // extern "C"
