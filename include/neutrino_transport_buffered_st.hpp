#pragma once

#include <atomic>
#include "neutrino_transport_buffered.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct buffered_singlethread_endpoint_t : public buffered_endpoint_t
            {
                using buffered_endpoint_t::buffered_endpoint_t;

                uint64_t m_frame_start{ 0 };

                bool consume(const uint8_t* p, const uint8_t* e) override;

            protected:
                bool flush() override;
            };
        }
    }
}
