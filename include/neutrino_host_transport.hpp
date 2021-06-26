#pragma once

#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct host_consumer_t : public consumer_t
            {
            };

            struct host_endpoint_impl_t : public endpoint_t
            {
                host_consumer_t& m_consumer;

                endpoint_impl_t(host_consumer_t& consumer)
                    : m_consumer(consumer) {}
            };

            namespace frame_v00
            {
                std::shared_ptr<host_endpoint_impl_t> create_endpoint_impl(known_encodings_t, consumer_t& consumer);
            }
        }
    }
}
