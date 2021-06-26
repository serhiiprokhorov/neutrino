#pragma once

#include "neutrino_transport.hpp"
#include <thread>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            /// associates endpoint to a unique id (port opened to a producer)
            struct endpoint_pool_t
            {
                enum class ENDPOINT_ACTION
                {
                    ALLOCATE
                    , RELEASE
                };

                struct endpoint_id_t
                {
                    typedef uint64_t value_type;
                    value_type m_id = 0;
                };

                struct enpoint_unique_id_generator_t
                {
                    std::atomic<endpoint_id_t::value_type> m_next = 0;
                    const endpoint_id_t generate() { return endpoint_id_t((++m_next).load()); }
                };

                enpoint_unique_id_generator_t m_endpoint_id_generator;

                typedef std::function< endpoint_t*(const endpoint_id_t /*connection id*/, ENDPOINT_ACTION) > endpoint_factory_t;
                endpoint_factory_t m_endpoint_factory;

                struct endpoint_guard_t
                {
                    endpoint_pool_t* m_pool;
                    endpoint_id_t m_id;
                    endpoint_t* m_endpoint;

                    endpoint_guard_t(endpoint_pool_t* pool, endpoint_id_t id)
                        : m_pool(pool), m_id(id), m_endpoint(m_endpoint_factory(id, ENDPOINT_ACTION::ALLOCATE))
                    {
                    }
                    endpoint_guard_t(const endpoint_guard_t&) = delete;
                    endpoint_guard_t(endpoint_guard_t&&) = delete;
                    endpoint_guard_t& operator=(const endpoint_guard_t&) = delete;
                    endpoint_guard_t& operator=(endpoint_guard_t&&) = delete;

                    ~endpoint_guard_t()
                    {
                        m_endpoint_factory(id, ENDPOINT_ACTION::RELEASE);
                    }
                };

                endpoint_guard_t allocate_endpoint()
                {
                    return endpoint_guard_t(this, m_endpoint_id_generator.generate());
                }

                virtual ~endpoint_pool_t() {}
                endpoint_pool_t(endpoint_factory_t endpoint_factory)
                    : m_endpoint_factory(endpoint_factory)
                {
                }
            };
        }
    }
}
