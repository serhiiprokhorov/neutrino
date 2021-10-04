/*
#include <cstring>
#include <cassert>

#include <neutrino_transport_buffered_exclusive.hpp>
#include <neutrino_transport_buffered_optimistic.hpp>

using namespace neutrino::impl::transport;

namespace neutrino
{
    namespace impl
    {
        namespace producer
        {
            const config_t configure_from_json(const char* cfg, const uint32_t cfg_bytes)
            {
                config_t ret;
                / *
            buffered_consumer_t::buffered_consumer_params_t bpo;
            bpo.m_message_buf_size = 1000;
            bpo.m_message_buf_watermark = 100;

            buffered_exclusive_consumer_t::buffered_exclusive_consumer_params_t po;

                * /
                return ret;
            }
        }
    }
}

extern "C"
{

void neutrino_producer_startup(const char* cfg, const uint32_t cfg_bytes)
{
    const auto cfg = producer::configure_from_json(cfg, cfg_bytes);

    std::unique_ptr<endpoint_consumer_t> endpoint;

    if(cfg.aep)
    {
        endpoint = std::make_shared<neutrino::impl::transport::async_posix_consumer_t>(cfg.ep);
    }
    else
    {
        return nullptr;
    }

    std::shared_ptr<buffered_consumer_t> ret;

    if(cfg.epo)
    {
        ret.reset(new buffered_exclusive_consumer_t(
                std::move(endpoint)
                , bpo
                , epo
                )
        );
    }
    else
    if (cfg.opo)
    {
        ret.reset(new buffered_optimistic_consumer_t>
                std::move(endpoint)
                , bpo
                , opo
                )
        );
    }

    neutrino::impl::transport::set_consumer(ret);
}

void neutrino_producer_shutdown()
{
    neutrino::impl::transport::set_consumer(std::shared_ptr<buffered_consumer_t>());
}

} // extern "C"
*/