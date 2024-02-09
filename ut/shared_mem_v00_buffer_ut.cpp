
#include <gtest/gtest.h>

#include <vector>
#include <string_view>
#include <algorithm>

#include <neutrino_shared_mem_v00_port_synchronized.hpp>
#include <neutrino_transport_shared_mem_v00_events.hpp>

using namespace neutrino::transport::shared_memory;

struct init_v00_buffers_ring_tests : public testing::Test
{

    struct producer_to_consumer_message_t
    {
        uint8_t x1 = 0;
        uint64_t x2 = 0;
        uint32_t x3 = 0;
    };

    std::vector<producer_to_consumer_message_t> m_expected_producer_message {
        { 1, 2, 3 },
        { 4, 5, 6 },
        { 7, 8, 9 },
        { 10, 11, 12 }
    };

    mem_buf_v00_linux_t m_consumer_memory;
    mem_buf_v00_linux_t m_producer_memory;

    void SetUp() override {

        m_consumer_memory=init_v00_buffers_ring("program=consumer,size=10000,");
        m_producer_memory=init_v00_buffers_ring("program=producer");

        std::copy(
            m_expected_producer_message.cbegin(), 
            m_expected_producer_message.cend(), 
            reinterpret_cast<producer_to_consumer_message_t*>(m_producer_memory.first->data())
            );
    }
};

TEST_F(init_v00_buffers_ring_tests, consumer_size_is_correct) {
    ASSERT_EQ(m_consumer_memory.first->size(), 10000);
}

TEST_F(init_v00_buffers_ring_tests, consumer_data_is_notnull) {
    ASSERT_TRUE(m_consumer_memory.first->data() != nullptr);
}

TEST_F(init_v00_buffers_ring_tests, consumer_fd_is_correct) {
    ASSERT_TRUE(m_consumer_memory.first->m_fd > 0);
}

TEST_F(init_v00_buffers_ring_tests, consumer_is_consumer) {
    ASSERT_TRUE(m_consumer_memory.first->m_is_consumer);
}

TEST_F(init_v00_buffers_ring_tests, producer_size_is_correct) {
    ASSERT_EQ(m_producer_memory.first->size(), 10000);
}

TEST_F(init_v00_buffers_ring_tests, producer_data_is_notnull) {
    ASSERT_TRUE(m_producer_memory.first->data() != nullptr);
}

TEST_F(init_v00_buffers_ring_tests, producer_fd_is_correct) {
    ASSERT_TRUE(m_producer_memory.first->m_fd > 0);
}

TEST_F(init_v00_buffers_ring_tests, producer_is_consumer_false) {
    ASSERT_FALSE(m_producer_memory.first->m_is_consumer);
}

TEST_F(init_v00_buffers_ring_tests, producer_communicates_to_consumer) {
    auto* consumer_data = reinterpret_cast<producer_to_consumer_message_t*>(m_consumer_memory.first->data());

    for(std::size_t cc = 0; cc < m_expected_producer_message.size(); cc++) {
        SCOPED_TRACE(std::to_string(cc));
        ASSERT_EQ(consumer_data[cc].x1, m_expected_producer_message[cc].x1);
        ASSERT_EQ(consumer_data[cc].x2, m_expected_producer_message[cc].->x2);
        ASSERT_EQ(consumer_data[cc].x3, m_expected_producer_message[cc].->x3);
    }
}
