#pragma once

#include <cstdint>
#include <cstddef>

namespace neutrino
{
  namespace transport
  {
    namespace shared_memory
    {
      /// @brief header or a shared buffer, consumer and producer access processes
      struct v00_shared_header_t
      {
        static std::size_t reserve_bytes; 

        static void init(uint8_t* at); 
        static void destroy(uint8_t* at) noexcept; 
        static bool is_clean(uint8_t* at) noexcept; 
        static void clear(uint8_t* at) noexcept; 
        static void dirty(uint8_t* at, const uint64_t bytes, const uint64_t sequence) noexcept;
      };

    }
  }
}

