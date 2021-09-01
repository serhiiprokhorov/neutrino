#include <memory>

#include <neutrino_transport_consumer_proxy_singleton.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace consumer
        {
            static std::shared_ptr<transport::consumer_proxy_t> proxy_singleton = std::shared_ptr<transport::consumer_proxy_t>();

            std::shared_ptr<transport::consumer_proxy_t> set_consumer_proxy(std::shared_ptr<transport::consumer_proxy_t> p) noexcept
            {
                proxy_singleton.swap(p);
                return p;
            }
            std::shared_ptr<transport::consumer_proxy_t> get_consumer_proxy() noexcept
            {
                return std::shared_ptr<transport::consumer_proxy_t>(proxy_singleton);
            }
        }
    }
}
