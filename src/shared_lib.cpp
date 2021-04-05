#include <memory>

#include <neutrino_producer.hpp>
#include <neutrino_transport.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace producer
        {
            static std::shared_ptr<transport::consumer_stub_t> active_consumer = std::shared_ptr<transport::consumer_stub_t>();

            std::shared_ptr<transport::consumer_stub_t> set_consumer(std::shared_ptr<transport::consumer_stub_t> p) noexcept
            {
                active_consumer.swap(p);
                return p;
            }
            std::shared_ptr<transport::consumer_stub_t> get_consumer() noexcept
            {
                return std::shared_ptr<transport::consumer_stub_t>(active_consumer);
            }
        }
    }
}
