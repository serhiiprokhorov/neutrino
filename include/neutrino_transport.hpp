#pragma once

#include <functional>
#include <memory>
#include "neutrino_consumer.hpp"

namespace neutrino
{
    template <typename PRODUCER_PORT>
    struct consumer_proxy_t : public consumer_t {
        std::unique_ptr<PRODUCER_PORT> m_port;

        consumer_proxy_t(std::unique_ptr<PRODUCER_PORT>&& port)
            : m_port(std::move(port))
            {}

        virtual void consume_checkpoint(
            const neutrino_nanoepoch_t& ne
            , const neutrino_stream_id_t& sid
            , const neutrino_event_id_t& eid
        ) {
            m_port->put<PRODUCER_PORT::EVENT_SET::event_checkpoint_t>(ne, sid, tid);
        };
        virtual void consume_context_enter(
            const neutrino_nanoepoch_t& ne
            , const neutrino_stream_id_t& sid
            , const neutrino_event_id_t& eid
        ) {
            m_port->put<PRODUCER_PORT::EVENT_SET::event_context_enter_t>(ne, sid, tid);
        };
        virtual void consume_context_leave(
            const neutrino_nanoepoch_t& ne
            , const neutrino_stream_id_t& sid
            , const neutrino_event_id_t& eid
        ) {
            m_port->put<PRODUCER_PORT::EVENT_SET::event_context_leave_t>(ne, sid, tid);
        };
        virtual void consume_context_exception(
            const neutrino_nanoepoch_t& ne
            , const neutrino_stream_id_t& sid
            , const neutrino_event_id_t& eid
        ) {
            m_port->put<PRODUCER_PORT::EVENT_SET::event_context_leave_t>(ne, sid, tid);
        };
    };

    /// @brief represents a producer at consumer's side;
    /// responsible to run a loop to accept ranges of bytes transported from consumer_proxy_t, deserialize them using the same logic 
    // and pass them to the given consumer
    struct producer_proxy_t
    {
        virtual ~producer_proxy_t() = default;
        /// @brief  calls consumer_t methods while processing incoming producer's events
        /// @param stop_cond a custom provided stop condition; the receiver stops processing the events when this condition is false
        /// @param consumer an imp of a consumer; the receiver calls corresponding methods from it
        virtual void process(std::function<bool()>stop_cond, consumer_t& consumer) = 0;
    };
}
