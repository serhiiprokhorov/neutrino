
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

struct when_producer_generates_event_synchronized : public testing::Test
{
    const std::size_t expected_checkpoint_offset = v00_shared_header_t::reserve_bytes;
    const neutrino_nanoepoch_t expected_ne_checkpoint {1};
    const neutrino_stream_id_t expected_sid_checkpoint {2};
    const neutrino_event_id_t expected_eid_checkpoint {3};

    const std::size_t expected_context_enter_offset = 
        v00_shared_header_t::reserve_bytes + 
        v00_events_set_t::event_checkpoint_t::bytes();
    const neutrino_nanoepoch_t expected_ne_context_enter {10};
    const neutrino_stream_id_t expected_sid_context_enter {20};
    const neutrino_event_id_t expected_eid_context_enter {30};

    const std::size_t expected_context_leave_offset = 
        v00_shared_header_t::reserve_bytes + 
        v00_events_set_t::event_checkpoint_t::bytes() + 
        v00_events_set_t::event_context_enter_t::bytes();
    const neutrino_nanoepoch_t expected_ne_context_leave {100};
    const neutrino_stream_id_t expected_sid_context_leave {200};
    const neutrino_event_id_t expected_eid_context_leave {300};

    const std::size_t expected_context_exception_offset = 
        v00_shared_header_t::reserve_bytes + 
        v00_events_set_t::event_checkpoint_t::bytes() + 
        v00_events_set_t::event_context_enter_t::bytes() + 
        v00_events_set_t::event_context_leave_t::bytes();
    const neutrino_nanoepoch_t expected_ne_context_exception {1000};
    const neutrino_stream_id_t expected_sid_context_exception {2000};
    const neutrino_event_id_t expected_eid_context_exception {3000};

    const std::size_t expected_occupied_bytes = v00_shared_header_t::reserve_bytes + 
        v00_shared_header_t::reserve_bytes + 
        v00_events_set_t::event_checkpoint_t::bytes() + 
        v00_events_set_t::event_context_enter_t::bytes() + 
        v00_events_set_t::event_context_leave_t::bytes() + 
        v00_events_set_t::event_context_exception_t::bytes();

    std::vector<uint8_t> m_memory;
    std::unique_ptr<buffers_ring_t<buffer_t<v00_shared_header_t, v00_events_set_t>>> m_ring;
    std::unique_ptr<synchronized_port_t<buffers_ring_t<buffer_t<v00_shared_header_t, v00_events_set_t>>>> m_port;


    void SetUp() override {

        // represents shared memory
        m_memory.resize(expected_occupied_bytes);

        // represents a ring of buffers over shared memory
        m_ring.reset(new buffers_ring_t<buffer_t<v00_shared_header_t, v00_events_set_t>>(
            m_memory.data(), 
            m_memory.size(), 
            1)
        );

        ASSERT_EQ(m_ring->cc_buffers(), 1);

        m_port.reset(new synchronized_port_t<buffers_ring_t<buffer_t<v00_shared_header_t, v00_events_set_t>>>( m_ring->get_first()));

        // expect port to place events into buffers one by one in exact order
        m_port->put<v00_events_set_t::event_checkpoint_t>(
                expected_ne_checkpoint,
                expected_sid_checkpoint,
                expected_eid_checkpoint
        );

        m_port->put<v00_events_set_t::event_context_enter_t>(
                expected_ne_context_enter,
                expected_sid_context_enter,
                expected_eid_context_enter
        );

        m_port->put<v00_events_set_t::event_context_leave_t>(
                expected_ne_context_leave,
                expected_sid_context_leave,
                expected_eid_context_leave
        );

        m_port->put<v00_events_set_t::event_context_exception_t>(
                expected_ne_context_exception,
                expected_sid_context_exception,
                expected_eid_context_exception
        );
    }

    void TearDown() override {
        m_port.reset();
    }
};

TEST_F(when_producer_generates_event_synchronized, then_event_checkpoint_is_correct) {
    const auto* actual_checkpoint = reinterpret_cast<v00_events_set_t::event_checkpoint_t*>(m_memory.data() + expected_checkpoint_offset);

    EXPECT_EQ(actual_checkpoint->m_ev, v00_events_set_t::EVENT::CHECKPOINT);
    EXPECT_EQ(actual_checkpoint->m_ne.v, expected_ne_checkpoint.v);
    EXPECT_EQ(actual_checkpoint->m_sid.v, expected_sid_checkpoint.v);
    EXPECT_EQ(actual_checkpoint->m_eid.v, expected_eid_checkpoint.v);
}

TEST_F(when_producer_generates_event_synchronized, then_event_context_enter_is_correct) {
    const auto* actual_context_enter = reinterpret_cast<v00_events_set_t::event_checkpoint_t*>(m_memory.data() + expected_context_enter_offset);

    EXPECT_EQ(actual_context_enter->m_ev, v00_events_set_t::EVENT::CONTEXT_ENTER);
    EXPECT_EQ(actual_context_enter->m_ne.v, expected_ne_context_enter.v);
    EXPECT_EQ(actual_context_enter->m_sid.v, expected_sid_context_enter.v);
    EXPECT_EQ(actual_context_enter->m_eid.v, expected_eid_context_enter.v);
}

TEST_F(when_producer_generates_event_synchronized, then_event_context_leave_is_correct) {
    const auto* actual_context_leave = reinterpret_cast<v00_events_set_t::event_checkpoint_t*>(m_memory.data() + expected_context_leave_offset);

    EXPECT_EQ(actual_context_leave->m_ev, v00_events_set_t::EVENT::CONTEXT_LEAVE);
    EXPECT_EQ(actual_context_leave->m_ne.v, expected_ne_context_leave.v);
    EXPECT_EQ(actual_context_leave->m_sid.v, expected_sid_context_leave.v);
    EXPECT_EQ(actual_context_leave->m_eid.v, expected_eid_context_leave.v);
}

TEST_F(when_producer_generates_event_synchronized, then_event_context_exception_is_correct) {
    const auto* actual_context_exception = reinterpret_cast<v00_events_set_t::event_checkpoint_t*>(m_memory.data() + expected_context_exception_offset);

    EXPECT_EQ(actual_context_exception->m_ev, v00_events_set_t::EVENT::CONTEXT_EXCEPTION);
    EXPECT_EQ(actual_context_exception->m_ne.v, expected_ne_context_exception.v);
    EXPECT_EQ(actual_context_exception->m_sid.v, expected_sid_context_exception.v);
    EXPECT_EQ(actual_context_exception->m_eid.v, expected_eid_context_exception.v);
}