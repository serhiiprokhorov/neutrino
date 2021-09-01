#pragma once

#include <functional>
#include "neutrino_consumer.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct endpoint_proxy_t
            {
                virtual ~endpoint_proxy_t() = default;

                virtual bool consume(const uint8_t*, const uint8_t*) { return false; };
                virtual bool flush() { return false; };
            };

            struct consumer_proxy_t : public consumer_t
            {
                endpoint_proxy_t& m_endpoint;

                consumer_proxy_t(endpoint_proxy_t& endpoint)
                    : m_endpoint(endpoint) {}
            };

            struct endpoint_t
            {
                consumer_t& m_consumer;

                endpoint_t(consumer_t& consumer)
                    : m_consumer(consumer) {}

                virtual void process(std::function<bool()>stop_cond) = 0;
            };
        }
    }
}
