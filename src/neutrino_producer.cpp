#include <chrono>
#include <memory>
#include <stdlib.h>

#include <string_view>

#include <neutrino_producer.h>


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
    const std::u8string_view cfg_view(cfg, cfg_bytes);

    try
    {
        if(cfg_view.find("transport=shared_mem") != std::string_view::npos) {
            if(cfg_view.find("platform=linux") != std::string_view::npos) {
                if(cfg_view.find("version=v00") != std::string_view::npos) {
                    if(cfg_view.find("sync=synchronized") != std::string_view::npos) {
                        neutrino::producer::configure::shared_mem_v00_synchronized_linux(cfg_view);
                    } else 
                    if(cfg_view.find("sync=exclusive") != std::string_view::npos) {
                        neutrino::producer::configure::shared_mem_v00_exclusive_linux(cfg_view);
                    } else 
                    if(cfg_view.find("sync=lockfree") != std::string_view::npos) {
                        neutrino::producer::configure::shared_mem_v00_lockfree_linux(cfg_view);
                    } else {
                        throw neutrino::helpers::unsupported_option("sync=...");
                    } 
                } else {
                    throw neutrino::helpers::unsupported_option("version=...");
                }
            } else {
                throw neutrino::helpers::unsupported_option("platform=...");
            }
        }
        throw neutrino::helpers::unsupported_option("transport=...");
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

} // extern "C"
