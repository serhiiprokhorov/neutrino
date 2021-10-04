#include <chrono>

#include <neutrino_transport_consumer_proxy_singleton.hpp>
namespace neutrino
{
  namespace impl
  {
    namespace transport
    {
      namespace
      {
        static endpoint_proxy_t dummy_endpoint_proxy;
        static std::shared_ptr<consumer_proxy_t> global_consumer_proxy{new consumer_proxy_t(dummy_endpoint_proxy)};
      }

      std::shared_ptr<consumer_proxy_t> set_consumer_proxy(std::shared_ptr<consumer_proxy_t> p) noexcept
      {
        global_consumer_proxy.swap(p);
        return p;
      }

      consumer_proxy_t& get_consumer_proxy() noexcept
      {
        return *global_consumer_proxy;
      }
    }
  }
}