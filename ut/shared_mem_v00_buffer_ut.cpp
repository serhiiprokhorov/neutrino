
#include <gtest/gtest.h>

#include <vector>
#include <string_view>
#include <algorithm>

#include <neutrino_shared_mem_v00_port_synchronized.hpp>
#include <neutrino_transport_shared_mem_v00_events.hpp>

using namespace neutrino::transport::shared_memory;

struct producer_consumer_buffers_connected : public testing::Test
{
    mem_buf_v00_linux_t m_consumer_memory;
    mem_buf_v00_linux_t m_producer_memory;

    const std::size_t m_expected_consumer_size = 10000;

    void SetUp() override {
        m_consumer_memory=init_v00_buffers_ring("process=consumer,size=10000,");
        m_producer_memory=init_v00_buffers_ring("process=producer");
    }
};

struct when_buffer_configured_as_consumer : public producer_consumer_buffers_connected {};

TEST_F(when_buffer_configured_as_consumer, then_consumer_size_match_config) {
    ASSERT_EQ(m_consumer_memory.first->size(), 10000);
}

TEST_F(when_buffer_configured_as_consumer, then_consumer_data_is_notnull) {
    ASSERT_TRUE(m_consumer_memory.first->data() != nullptr);
}

TEST_F(when_buffer_configured_as_consumer, then_consumer_fd_is_not_null) {
    ASSERT_TRUE(m_consumer_memory.first->m_fd > 0);
}

TEST_F(when_buffer_configured_as_consumer, then_is_consumer_true) {
    ASSERT_TRUE(m_consumer_memory.first->m_is_consumer);
}

struct when_buffer_configured_as_producer : public producer_consumer_buffers_connected {};

TEST_F(when_buffer_configured_as_producer, then_producer_size_match_to_consumer_size) {
    ASSERT_EQ(m_producer_memory.first->size(), m_consumer_memory.first->size());
}

TEST_F(when_buffer_configured_as_producer, then_producer_data_is_notnull) {
    ASSERT_TRUE(m_producer_memory.first->data() != nullptr);
}

TEST_F(when_buffer_configured_as_producer, then_producer_fd_is_not_null) {
    ASSERT_TRUE(m_producer_memory.first->m_fd > 0);
}

//TEST_F(when_buffer_configured_as_producer, then_producer_fd_is_different_than_consumer_fd) {
//    ASSERT_NE(m_producer_memory.first->m_fd, m_consumer_memory.first->m_fd);
//}

TEST_F(when_buffer_configured_as_producer, then_is_consumer_false) {
    ASSERT_FALSE(m_producer_memory.first->m_is_consumer);
}

struct when_producer_puts_data : public producer_consumer_buffers_connected {
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

    void SetUp() override {

        producer_consumer_buffers_connected::SetUp();

        std::copy(
            m_expected_producer_message.cbegin(), 
            m_expected_producer_message.cend(), 
            reinterpret_cast<producer_to_consumer_message_t*>(m_producer_memory.first->data())
            );
    }

};

TEST_F(when_producer_puts_data, then_consumer_sees_same_data) {
    auto* consumer_data = reinterpret_cast<producer_to_consumer_message_t*>(m_consumer_memory.first->data());

    for(std::size_t cc = 0; cc < m_expected_producer_message.size(); cc++) {
        SCOPED_TRACE(std::to_string(cc));
        ASSERT_EQ(consumer_data[cc].x1, m_expected_producer_message[cc].x1);
        ASSERT_EQ(consumer_data[cc].x2, m_expected_producer_message[cc].x2);
        ASSERT_EQ(consumer_data[cc].x3, m_expected_producer_message[cc].x3);
    }
}
