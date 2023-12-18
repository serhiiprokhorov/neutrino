#include <chrono>

#include <transport/neutrino_transport_consumer_proxy_singleton.hpp>

namespace neutrino
{
    namespace transport
    {
      namespace
      {
        // make sure this class does nothing, consuming events before the first call to set_consumer_proxy
        struct consumer_proxy_noop_t : public consumer_proxy_t 
        {

        };
        
        static std::unique_ptr<consumer_proxy_t> global_consumer_proxy{new consumer_proxy_noop_t()};
      }

      std::unique_ptr<consumer_proxy_t> set_consumer_proxy(std::unique_ptr<consumer_proxy_t> p) noexcept
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