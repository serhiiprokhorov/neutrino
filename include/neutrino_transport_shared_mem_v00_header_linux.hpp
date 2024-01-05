#pragma once

#include <memory>
#include <stdexcept>
#include <cstddef> 
#include <cstdint> 
#include <atomic>

#include <semaphore.h>

namespace neutrino
{
  namespace transport
  {
    namespace shared_memory
    {
      /// @brief header or a shared buffer, consumer and producer access processes
      struct alignas(uint64_t) v00_shared_header_t
      {
        /// states:
        /// - "blocked/signalled" (value ==0) consumer waits, producer adds events    
        /// - "unblocked/not signaled" (value >0) consumer reads the events, producer tries if the buffer is free
        sem_t m_ready; 
        /// filled by dirty(), tells consumer how many bytes are occupied by events data;
        /// needed because the buffer may include some different amount of bytes, 
        /// the buffer is signalled when there is not enough space to put a biggest event (hi watermark is reached)
        /// states:
        /// - ==0 the buffer is clean, producer may add new events (a producer maintains its own counter of bytes available), consumer is blocked
        /// - >0 the buffer is dirty/signaled, producer may not touch the buffer, consumer process the events 
        std::atomic_uint64_t m_inuse_bytes = 0; 
        /// indicates an order in which buffers are marked "ready to be consumed", 
        /// needed to help resolve ambiguity consumer side if signals were delayed or processed in out of order
        std::atomic_uint64_t m_sequence = 0;

        v00_shared_header_t(const bool is_consumer) {
          if(is_consumer) {
            if( sem_init(&m_ready, 1 /* this sem is shared between processes */, 1) != 0 ) {
              error(errno, "format_at.sem_init");
            }
          }
        }

        v00_shared_header_t(const v00_shared_header_t&) = delete;
        v00_shared_header_t(v00_shared_header_t&)& = delete;
        v00_shared_header_t& operator=(const v00_shared_header_t&) = delete;
        v00_shared_header_t& operator=(v00_shared_header_t&&) = delete;

        void format(bool is_new) noexcept; 
        void init() noexcept; 
        void destroy() noexcept; 
        bool is_clean() noexcept; 
        void clear() noexcept; 
        void dirty(const uint64_t bytes, const uint64_t sequence) noexcept;
      };

    }
  }
}

