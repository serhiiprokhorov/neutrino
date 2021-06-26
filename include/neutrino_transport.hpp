#pragma once

#include <memory>
#include <functional>

#include "neutrino_frames_local.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            /*
            {host neutrino application
                cmd line args -> path to client application
                opens anon pipe, keeps read end
                starts and monitors client application, passes write end to a client app
                exits when client applicaion ends
                {C1 is an instance of consumer_t,
                    implements general processing logic}
                {E1 is an instance of endpoint_impl_t,
                    injected with C
                    uses neutrino transport to decode binary stream - native platform encoding
                }
            }
            {client application
              {uses neutrino "C" interface
                {E2 is an instance of endpoint_t, communicates with host neutrino application via anon pipe P}
                {singleton C2, instance of consumer_stub_t, injected with E2
                    uses neutrino transport to encode binary stream - native platform encoding
                }
              }
            }
            */
            struct endpoint_t
            {
                virtual ~endpoint_t() = default;

                virtual std::ptrdiff_t consume(const uint8_t*, const uint8_t*) { return 0; };
                virtual bool flush() { return false; };
            };

            struct consumer_t
            {
                virtual ~consumer_t() = default;
                virtual void consume_checkpoint(
                    const local::payload::nanoepoch_t::type_t&
                    , const local::payload::stream_id_t::type_t&
                    , const local::payload::event_id_t::type_t&
                ) {};
                virtual void consume_context(
                    const local::payload::nanoepoch_t::type_t&
                    , const local::payload::stream_id_t::type_t&
                    , const local::payload::event_id_t::type_t&
                    , const local::payload::event_type_t::event_types&
                ) {};
            };

            struct consumer_stub_t : public consumer_t
            {
                endpoint_t& m_endpoint;

                consumer_stub_t(endpoint_t& endpoint)
                    : m_endpoint(endpoint) {}
            };

            struct endpoint_impl_t : public endpoint_t
            {
                consumer_t& m_consumer;

                endpoint_impl_t(consumer_t& consumer)
                    : m_consumer(consumer) {}
            };

            namespace frame_v00
            {
                enum class known_encodings_t
                {
                    BINARY_NETWORK
                    , BINARY_NATIVE // for localhost
                    , JSON
                };

                std::shared_ptr<consumer_stub_t> create_consumer_stub(known_encodings_t, endpoint_t& endpoint);
                std::shared_ptr<endpoint_impl_t> create_endpoint_impl(known_encodings_t, consumer_t& consumer);
            }
        }
    }
}
