#pragma once

#include <tuple>

#include "neutrino_frames_serialized.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace serialized
        {
            template <>
            struct raw_t<local::payload::header_t, native_byte_order_target_t>
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
            struct raw_t<local::payload::nanoepoch_t, native_byte_order_target_t>
                : public raw_base_t<local::payload::nanoepoch_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& n) noexcept
                {
                    memcpy(&n, p, span());
                    return true;
                };
                static uint8_t* convert(const local_type_t n, uint8_t* p) noexcept
                {
                    memcpy(p, &n, span());
                    return p + span();
                };
            };

            template <>
            struct raw_t<local::payload::stream_id_t, native_byte_order_target_t>
                : public raw_base_t<local::payload::stream_id_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& n) noexcept
                {
                    memcpy(&n, p, span());
                    return true;
                };
                static uint8_t* convert(const local_type_t n, uint8_t* p) noexcept
                {
                    memcpy(p, &n, span());
                    return p + span();
                };
            };

            template <>
            struct raw_t<local::payload::event_id_t, native_byte_order_target_t>
                : public raw_base_t<local::payload::event_id_t>
            {
                static constexpr const std::size_t span() { return sizeof(local_type_t); }

                static bool convert(const uint8_t* p, local_type_t& n) noexcept
                {
                    memcpy(&n, p, span());
                    return true;
                };
                static uint8_t* convert(const local_type_t n, uint8_t* p) noexcept
                {
                    memcpy(p, &n, span());
                    return p + span();
                };
            };

            template <>
            struct raw_t<local::payload::event_type_t, native_byte_order_target_t>
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
