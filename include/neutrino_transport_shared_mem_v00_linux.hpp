#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <optional>

#include "neutrino_types.h"
#include "neutrino_transport_shared_mem.hpp"

namespace neutrino
{
  namespace transport
  {
    namespace shared_memory
    {
      /// @brief wrapper struct includes al known events definitions and metadata about them
      struct v00_events_set_t {
        enum class EVENT : std::uint64_t {
          CHECKPOINT = 1,
          CONTEXT_ENTER,
          CONTEXT_LEAVE,
          CONTEXT_EXCEPTION
        };

        struct alignas(uint64_t) event_base_t {
          EVENT m_ev;
          neutrino_nanoepoch_t m_ne;
          neutrino_stream_id_t m_sid;
          neutrino_event_id_t m_eid;

          static constexpr std::size_t bytes = sizeof(event_base_t);

          event_base_t(
            EVENT ev,
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : m_ev(ev), m_ne(*ne), m_sid(*sid), m_eid(*eid)
          {
          }
        };

        struct alignas(uint64_t) event_checkpoint_t : public event_base_t {
          event_checkpoint_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CHECKPOINT, ne, sid, eid) {}
        };

        struct event_context_enter_t : public event_base_t {
          event_context_enter_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CONTEXT_ENTER, ne, sid, eid) {}
        };

        struct event_context_leave_t : public event_base_t {
          event_context_leave_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CONTEXT_LEAVE, ne, sid, eid) {}
        };

        struct event_context_exception_t : public event_base_t {
          event_context_exception_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CONTEXT_EXCEPTION, ne, sid, eid) {}
        };

        static constexpr std::size_t biggest_event_size_bytes = 
          std::max({event_checkpoint_t::bytes, event_context_enter_t::bytes, event_context_leave_t::bytes, event_context_exception_t::bytes});
      };

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

        v00_shared_header_t() {
          if( sem_init(&m_ready, 1 /* this sem is shared between processes */, 1) != 0 ) {
            error(errno, "format_at.sem_init");
          }
        }

        void format(bool is_new) noexcept; 
        void destroy() noexcept; 
        bool is_clean() noexcept; 
        void clear() noexcept; 
        void dirty(const uint64_t bytes, const uint64_t sequence) noexcept;
      };

    }
  }
}

