#pragma once

#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct async_posix_consumer_t : public endpoint_t
            {
                struct async_posix_consumer_params_t
                {
                };

                async_posix_consumer_params_t m_params;

                async_posix_consumer_t(const async_posix_consumer_params_t asp/*, const endpoint_consumer_t::endpoint_params_t p*/)
                    : m_params(asp), endpoint_t(/*p*/)
                {
                }

                bool consume(const uint8_t* p, const uint8_t* e) final
                {
                    return false;
                }
            };
        }
    }
}
