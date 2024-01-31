#pragma once

#include <cstdint> 
#include <algorithm>

#include "neutrino_types.h"

namespace neutrino
{
  namespace transport
  {
    namespace shared_memory
    {
      /// @brief wrapper struct includes al known events definitions and metadata about them
      struct v00_events_set_t {
        enum class EVENT : uint64_t {
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

          event_base_t(
            EVENT ev,
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : m_ev(ev), m_ne(ne), m_sid(sid), m_eid(eid)
          {
          }
        };

        struct alignas(uint64_t) event_checkpoint_t : public event_base_t {
          event_checkpoint_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CHECKPOINT, ne, sid, eid) {}
          static constexpr std::size_t bytes() { return sizeof(event_base_t); };
        };

        struct event_context_enter_t : public event_base_t {
          event_context_enter_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CONTEXT_ENTER, ne, sid, eid) {}
          static constexpr std::size_t bytes() { return sizeof(event_base_t); };
        };

        struct event_context_leave_t : public event_base_t {
          event_context_leave_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CONTEXT_LEAVE, ne, sid, eid) {}
          static constexpr std::size_t bytes() { return sizeof(event_base_t); };
        };

        struct event_context_exception_t : public event_base_t {
          event_context_exception_t(
            const neutrino_nanoepoch_t& ne,
            const neutrino_stream_id_t& sid,
            const neutrino_event_id_t& eid
          ) : event_base_t(EVENT::CONTEXT_EXCEPTION, ne, sid, eid) {}
          static constexpr std::size_t bytes() { return sizeof(event_base_t); };
        };

        static constexpr std::size_t biggest_event_size_bytes() { 
          return std::max({event_checkpoint_t::bytes(), event_context_enter_t::bytes(), event_context_leave_t::bytes(), event_context_exception_t::bytes()});
        }
      };

    }
  }
}

