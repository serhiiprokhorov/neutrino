
#include <gtest/gtest.h>

#include <string_view>

#include <neutrino_shared_mem_v00_port_synchronized.hpp>
#include <neutrino_transport_shared_mem_v00_events.hpp>

using namespace neutrino::transport::shared_memory;

namespace neutrino::producer::configure
{
void shared_mem_v00_exclusive_linux(std::basic_string_view<char, std::char_traits<char> > const&)
{
}

void shared_mem_v00_lockfree_linux(std::basic_string_view<char, std::char_traits<char> > const&)
{
}
}

struct SynchronizedTest : public testing::Test
{
    // shared memory mapped at consumer side
    mem_buf_v00_linux_t m_consumer_membuf;
    // producer port
    std::unique_ptr<synchronized_port_v00_linux_t> m_port;

    void SetUp() override {
        
        // this creates shared mem and exports corresponding fd via env variable
        m_consumer_membuf = init_v00_buffers_ring("process=consumer");


        // this opens shared mem via env variable
        m_port = std::make_unique<synchronized_port_v00_linux_t>(
            init_v00_buffers_ring("process=producer")
        );
    }

    void TearDown() override {
        m_port.reset();
    }
};

TEST_F(SynchronizedTest, test1) {
    using namespace neutrino::transport::shared_memory;

    const neutrino_nanoepoch_t expected_ne {1};
    const neutrino_stream_id_t expected_sid {2};
    const neutrino_event_id_t expected_eid {3};

    m_port->put<v00_events_set_t::event_checkpoint_t>(
            expected_ne,
            expected_sid,
            expected_eid
    );
}