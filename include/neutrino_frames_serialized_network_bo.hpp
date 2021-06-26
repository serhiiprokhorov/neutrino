#pragma once

#include "neutrino_frames_serialized.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace serialized
        {
            // https://stackoverflow.com/questions/809902/64-bit-ntohl-in-c

            template <typename local_type_t>
            uint8_t* to_network(const local_type_t& h, uint8_t* p) = delete;
            template <typename local_type_t>
            bool from_network(const uint8_t* p, local_type_t& h) = delete;

            // build config must provide an platform-specific implementation
            template <>
            uint8_t* to_network(const uint64_t& h, uint8_t* p);

            template <>
            bool from_network(const uint8_t* p, uint64_t& h);

            template <>
            struct raw_t<local::payload::header_t, network_byte_order_target_t>
                : public raw_base_t<local::payload::header_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& h) noexcept
                {
                    h = *p;
                    return true;
                };
                static uint8_t* convert(const local_type_t h, uint8_t* p) noexcept
                {
                    *p = h;
                    return p + span();
                };
            };

            template <>
            struct raw_t<local::payload::nanoepoch_t, network_byte_order_target_t>
                : public raw_base_t<local::payload::nanoepoch_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& n) noexcept
                {
                    return from_network(p, n);
                };
                static uint8_t* convert(const local_type_t n, uint8_t* p) noexcept
                {
                    return to_network(n, p);
                };
            };

            template <>
            struct raw_t<local::payload::stream_id_t, network_byte_order_target_t>
                : public raw_base_t<local::payload::stream_id_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& n) noexcept
                {
                    return from_network(p, n);
                };
                static uint8_t* convert(const local_type_t n, uint8_t* p) noexcept
                {
                    return to_network(n, p);
                };
            };

            template <>
            struct raw_t<local::payload::event_id_t, network_byte_order_target_t>
                : public raw_base_t<local::payload::event_id_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& n) noexcept
                {
                    return from_network(p, n);
                };
                static uint8_t* convert(const local_type_t n, uint8_t* p) noexcept
                {
                    return to_network(n, p);
                };
            };

            template <>
            struct raw_t<local::payload::event_type_t, network_byte_order_target_t>
                : public raw_base_t<local::payload::event_type_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& e) noexcept { e = *p; return true; };
                static uint8_t* convert(const local::payload::event_type_t::event_types e, uint8_t* p) noexcept
                {
                    *p = static_cast<local_type_t>(e);
                    return p + span();
                };
            };
        }
    }
}
