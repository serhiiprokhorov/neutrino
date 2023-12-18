#pragma once

#include <memory>
#include <neutrino_transport.hpp>

namespace neutrino
{
    namespace transport
    {
        /// @brief configures the proxy for a producer; events produced before the first call are ignored 
        /// @param p new proxy
        /// @return old proxy
        std::unique_ptr<consumer_proxy_t> set_consumer_proxy(std::unique_ptr<consumer_proxy_t> p) noexcept;
        /// @return currently configured proxy
        consumer_proxy_t& get_consumer_proxy() noexcept;
    }
}
