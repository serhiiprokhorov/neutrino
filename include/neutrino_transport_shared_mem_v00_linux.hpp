#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <optional>

#include "neutrino_transport_shared_mem.hpp"

namespace neutrino
{
  namespace transport
  {
    namespace shared_memory
    {
      namespace linux
      {

        /// @brief a tool to allocate/mmap a range of bytes as a shared memory;
        /// uses memfd_* family of functions to create/open in-memory file and mmap this file 
        struct initializer_memfd_t
        {
          initializer_memfd_t(std::size_t buffer_bytes); /// consumer uses this ctor to create a shared memory
          initializer_memfd_t(unsigned int fd); /// producer uses this ctor to connect to already existing shared memory (fd is inherited from consumer)
          ~initializer_memfd_t();

          /// @return ptr to the first byte of a shared memory or null if not initialized
          uint8_t* data() { return m_rptr; }
          /// @return a size in bytes of a shared memory
          std::size_t size() const { return m_bytes; }
            
          unsigned int m_fd = -1; /// fd of memfd
          uint8_t* m_rptr = nullptr; /// mmap shared mem ptr
          std::size_t m_bytes = 0; /// size in bytes of a single buffer
        };

        /// @brief binds together types/functions/constants related to v00 shared header;
        /// one purpose is to simplify calculations on a set of events
        struct v00_shared_header_control_t {

          enum class EVENT : std::uint64_t {
            CHECKPOINT = 1,
            CONTEXT_ENTER,
            CONTEXT_LEAVE,
            CONTEXT_EXCEPTION
          }

          /// @brief wrapper struct includes al known events definitions and metadata about them
          struct events_set_t {

            struct alignas(uint64_t) event_base_t {
              EVENT m_ev;
              neutrino_nanoepoch_t m_ne;
              neutrino_stream_id_t m_sid;
              neutrino_event_id_t m_eid;
              static constexpr std::size_t bytes = sizeof(event_base_t);
              void event_into(uint8_t* start, const EVENT ev) {
                  memcpy(start, &ev, sizeof(ev));
                  memcpy(start + sizeof(ev), this, sizeof(this));
              }
            };

            struct alignas(uint64_t) event_checkpoint_t : public event_base_t {
              using event_base_t::event_base_t;

              void into(uint8_t* start) {
                  event_into(start, EVENT::CHECKPOINT);
              }
            };

            struct event_context_enter_t : public event_base_t {
              using event_base_t::event_base_t;

              void into(uint8_t* start) {
                  event_into(start, EVENT::CONTEXT_ENTER);
              }
            };

            struct event_context_leave_t : public event_base_t {
              using event_base_t::event_base_t;

              void into(uint8_t* start) {
                  event_into(start, EVENT::CONTEXT_LEAVE);
              }
            };

            struct event_context_exception_t : public event_base_t {
              using event_base_t::event_base_t;

              void into(uint8_t* start) {
                  event_into(start, EVENT::CONTEXT_EXCEPTION);
              }
            };

            static constexpr std::size_t biggest_event_size_bytes = std::max({event_checkpoint_t::bytes, event_context_t::bytes});
          };

          /// @brief header or a shared buffer, consumer and producer access processes
          struct alignas(uint64_t) header_t
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

            void format(bool is_new) noexcept; 
            void destroy() noexcept; 
            bool is_clean() noexcept; 
            void clear() noexcept; 
            void dirty(const uint64_t bytes, const uint64_t sequence) noexcept;
          };

          /// @brief how many bytes can
          const std::size_t smallest_buffer_size_bytes = sizeof(shared_header_t) + events_set_t::biggest_event_size_bytes;

          /// @brief this struct aggregates shared header ptr and corresponding buffer sizes
          struct header_control_t {
            shared_header_t * m_header = nullptr;
            uint8_t* m_first_available = nullptr;
            uint8_t* m_hi_water_mark = nullptr;
            uint8_t* m_end = nullptr;
          };

          /// format helper; 
          /// @return header ptr or nullptr if not enough space or unable to initialize semaphore
          header_control_t format_at(uint8_t* start, bool create_new, std::size_t bytes_available) {

            if(smallest_buffer_size_bytes > bytes_available)
              return header_control_t{};


            /// try initialize header over given position
            header_control_t ret{
              .m_header = new (start) shared_header_t,
              .m_first_available = start + sizeof(shared_header_t),
              .m_hi_water_mark = start + bytes_available - events_set_t::biggest_event_size_bytes,
              .m_end = start + bytes_available,
            };

            if(create_new) {
              if( sem_init(&(ret.m_header->m_ready), 1 /* this sem is shared between processes */, 1) != 0 ) {
                error(errno, "format_at.sem_init");
              }
            }

            return ret;
          }
        };
      }
    }
  }
}

