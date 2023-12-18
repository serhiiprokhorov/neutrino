#include <chrono>

#include <neutrino_transport_consumer_proxy_singleton.hpp>
#include <neutrino_mappers.hpp>

using namespace neutrino;

extern "C"
{

neutrino_nanoepoch_t neutrino_nanoepoch(void)
{
    return neutrino_nanoepoch_t{ .v = 
        uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
    };
}

void neutrino_checkpoint(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    transport::get_consumer_proxy().consume_checkpoint(nanoepoch, stream_id, event_id);
}

void neutrino_context_enter(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    transport::get_consumer_proxy().consume_context_enter(nanoepoch, stream_id, event_id);
}

void neutrino_context_leave(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    transport::get_consumer_proxy().consume_context_leave(nanoepoch, stream_id, event_id);
}

void neutrino_context_exception(const neutrino_nanoepoch_t nanoepoch, const neutrino_stream_id_t stream_id, const neutrino_event_id_t event_id)
{
    transport::get_consumer_proxy().consume_context_exception(nanoepoch, stream_id, event_id);
}

void neutrino_flush()
{
    //transport::get_consumer_proxy().m_endpoint.flush();
}

} // extern "C"
