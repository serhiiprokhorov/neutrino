#pragma once

#include <tuple>

#include "neutrino_frames_local.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace serialized
        {
            template <typename _local_t>
            struct raw_base_t
            {
                typedef _local_t local_t;
                typedef typename _local_t::type_t local_type_t;
            };

            template <typename _local_t, typename byte_order_t>
            struct raw_t : public raw_base_t<_local_t>
            {
                static constexpr std::size_t span() = delete;
                static bool convert(const uint8_t* p, local_type_t& h) noexcept = delete;
                static uint8_t* convert(const local_type_t h, uint8_t* p) noexcept = delete;
            };

            struct native_byte_order_target_t {};
            struct network_byte_order_target_t {};
        }
    }
}
