#pragma once

#include <memory>
#include <neutrino_transport.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            std::shared_ptr<consumer_proxy_t> set_consumer_proxy(std::shared_ptr<consumer_proxy_t> p) noexcept;
            std::shared_ptr<consumer_proxy_t> get_consumer_proxy() noexcept;
        }
    }
}
