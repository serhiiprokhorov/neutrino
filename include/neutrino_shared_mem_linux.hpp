#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <source_location>

namespace neutrino
{
  namespace transport
  {
    namespace shared_memory
    {
      /// @brief a tool to allocate/mmap a range of bytes as a shared memory;
      /// uses memfd_* family of functions to create/open in-memory file and mmap this file 
      struct initializer_memfd_t final
      {
        struct errno_error : public std::runtime_error
        {
          int errno_captured;
          std::source_location location_captured;
          errno_error(std::source_location loc, int err, const char* what)
          : errno_captured(err), location_captured(loc), std::runtime_error(what)
          {}
        };

        initializer_memfd_t(std::size_t buffer_bytes); /// consumer uses this ctor to create a shared memory
        initializer_memfd_t(unsigned int fd); /// producer uses this ctor to connect to already existing shared memory (fd is inherited from consumer)
        ~initializer_memfd_t();

        /// @return ptr to the first byte of a shared memory or null if not initialized
        uint8_t* data() { return m_rptr; }
        /// @return a size in bytes of a shared memory
        std::size_t size() const { return m_bytes; }
          
        unsigned int m_fd; /// fd of memfd
        uint8_t* m_rptr; /// mmap shared mem ptr
        std::size_t m_bytes; /// size in bytes of a single buffer
      };
    }
  }
}

