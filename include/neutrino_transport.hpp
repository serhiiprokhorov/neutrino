#pragma once

#include <functional>
#include <memory>
#include "neutrino_consumer.hpp"

namespace neutrino
{
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
