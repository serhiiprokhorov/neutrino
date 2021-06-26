#include <WinSock2.h>
#include <neutrino_frames_serialized_network_bo.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace serialized
        {
            static const std::size_t uint64_t_serialized_span = sizeof(uint64_t);
            template <>
            uint8_t* to_network(const uint64_t& h, uint8_t* p)
            {
                const uint64_t n = htonll(h);
                memcpy(p, &n, uint64_t_serialized_span);
                return p + uint64_t_serialized_span;
            }

            template <>
            bool from_network(const uint8_t* p, uint64_t& h)
            {
                h = ntohll(*reinterpret_cast<const uint64_t*>(p));
                return true;
            }
        }
    }
}
