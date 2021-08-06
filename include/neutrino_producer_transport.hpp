#pragma once

#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct guest_endpoint_t : public endpoint_t
            {
            };

            struct guest_consumer_t : public consumer_t
            {
                guest_endpoint_t& m_endpoint;

                consumer_stub_t(guest_endpoint_t& endpoint)
                    : m_endpoint(endpoint) {}
            };

            namespace frame_v00
            {
                std::shared_ptr<guest_consumer_t> create_consumer_stub(known_encodings_t, endpoint_t& endpoint);
            }
        }
    }
}
