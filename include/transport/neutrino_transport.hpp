#pragma once

#include <functional>
#include "../neutrino_consumer.hpp"

namespace neutrino
{
    namespace transport
    {
        /// @brief represents a consumer at producer's side; 
        /// responsible to serialize consumer events into a range of bytes and to send this range to a consumer 
        struct consumer_proxy_t : public consumer_t
        {
            /// @brief sends the range of bytes to a producer
            /// @param b the first byte of the range
            /// @param e the first byte past the range (e-1 is the last byte of the range) 
            /// @return true when the range has been scheduled successfully , false otherwise
            virtual bool schedule_for_transport(const uint8_t* b, const uint8_t* e) { return false; };

            
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
}
