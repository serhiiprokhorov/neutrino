#include <gtest/gtest.h>
#include <thread>
//#include <neutrino_mock.hpp>

#include <neutrino_transport.hpp>
#include <neutrino_producer.hpp>

using namespace neutrino::impl;

TEST(neutrino_nanoepoch, is_linear)
{
    auto x1 = neutrino_nanoepoch();
    auto x2 = neutrino_nanoepoch();
    auto x3 = neutrino_nanoepoch();

    ASSERT_TRUE(x3 > x2);
    ASSERT_TRUE(x2 > x1);
}

TEST(neutrino_nanoepoch, interval)
{
    auto sleep_nanoseconds = std::chrono::nanoseconds(std::chrono::milliseconds(200));
    auto sleep_nanoseconds_max = sleep_nanoseconds + std::chrono::milliseconds(200); // granularity is unknown, assume worst
    auto x1 = neutrino_nanoepoch();
    std::this_thread::sleep_for(sleep_nanoseconds);
    auto x2 = neutrino_nanoepoch();

    ASSERT_TRUE((x2 - x1) >= (uint64_t)sleep_nanoseconds.count());
    ASSERT_TRUE((x2 - x1) < (uint64_t)sleep_nanoseconds_max.count());
}
/*
struct neutrino_general_workflow_tests : public ::testing::Test
{
    const uint64_t checkpoint_id_1 = 1;
    const uint64_t checkpoint_id_2 = 2;

    const uint64_t nanoepoch_1 = 101;
    const uint64_t nanoepoch_2 = 102;
    const uint64_t nanoepoch_3 = 103;
    const uint64_t nanoepoch_4 = 104;

    const uint64_t stream_id_1 = 301;
    const uint64_t stream_id_2 = 302;

    template <transport::frame_v00::known_encodings_t transport_encoding>
    struct channel_t
    {
        std::shared_ptr<neutrino::mock::connection_t<neutrino::impl::transport::endpoint_impl_t>> m_connection;
        std::shared_ptr<transport::endpoint_impl_t> m_endpoint_impl;
        std::shared_ptr<transport::consumer_stub_t> m_consumer_stub;

        channel_t(neutrino::mock::consumer_t& consumer)
        {
            m_endpoint_impl = transport::frame_v00::create_endpoint_impl(
                transport_encoding, consumer);

            m_connection.reset(new neutrino::mock::connection_t<neutrino::impl::transport::endpoint_impl_t>(*m_endpoint_impl));

            m_consumer_stub = transport::frame_v00::create_consumer_stub(
                transport_encoding, *m_connection);
        }
    };

    template <transport::frame_v00::known_encodings_t transport_encoding>
    struct channel_guard_t
    {
        std::shared_ptr<channel_t<transport_encoding>> m_channel;
        channel_guard_t(neutrino::mock::consumer_t& mock_consumer)
        {
            m_channel.reset(new channel_t<transport_encoding>(mock_consumer));
        }
    };

    std::shared_ptr<neutrino::mock::consumer_t> m_mock_consumer;

    void SetUp() final
    {
        m_mock_consumer.reset(new neutrino::mock::consumer_t());
    }

    void TearDown() final
    {
        ASSERT_TRUE(m_mock_consumer.get());
        ASSERT_TRUE(m_mock_consumer->m_expected_checkpoints.empty());
        ASSERT_TRUE(m_mock_consumer->m_expected_contexts.empty());
        m_mock_consumer.reset();
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_checkpoint_same_stream()
    {
        SCOPED_TRACE(__FUNCTION__);
        m_mock_consumer->
            expect_checkpoint(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_checkpoint(nanoepoch_2, stream_id_1, checkpoint_id_2);

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            ASSERT_NO_THROW(neutrino_checkpoint(nanoepoch_1, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_checkpoint(nanoepoch_2, stream_id_1, checkpoint_id_2));
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_checkpoint_different_stream()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_checkpoint(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_checkpoint(nanoepoch_2, stream_id_2, checkpoint_id_2);

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            ASSERT_NO_THROW(neutrino_checkpoint(nanoepoch_1, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_checkpoint(nanoepoch_2, stream_id_2, checkpoint_id_2));
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_context_enter_leave_same_stream()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_context_leave(nanoepoch_2, stream_id_1, checkpoint_id_1);

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            ASSERT_NO_THROW(neutrino_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_leave(nanoepoch_2, stream_id_1, checkpoint_id_1));
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_context_enter_leave_interleaved_stream()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_context_enter(nanoepoch_1, stream_id_2, checkpoint_id_1)
            .expect_context_leave(nanoepoch_2, stream_id_1, checkpoint_id_1)
            .expect_context_leave(nanoepoch_2, stream_id_2, checkpoint_id_1)
            ;

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            ASSERT_NO_THROW(neutrino_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_enter(nanoepoch_1, stream_id_2, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_leave(nanoepoch_2, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_leave(nanoepoch_2, stream_id_2, checkpoint_id_1));
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_context_enter_panic_same_stream()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_context_panic(nanoepoch_2, stream_id_1, checkpoint_id_1);

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            ASSERT_NO_THROW(neutrino_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_panic(nanoepoch_2, stream_id_1, checkpoint_id_1));
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_context_enter_panic_interleaved_stream()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_context_enter(nanoepoch_2, stream_id_2, checkpoint_id_1)
            .expect_context_panic(nanoepoch_3, stream_id_1, checkpoint_id_1)
            .expect_context_panic(nanoepoch_4, stream_id_2, checkpoint_id_1)
            ;

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            ASSERT_NO_THROW(neutrino_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_enter(nanoepoch_2, stream_id_2, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_panic(nanoepoch_3, stream_id_1, checkpoint_id_1));
            ASSERT_NO_THROW(neutrino_context_panic(nanoepoch_4, stream_id_2, checkpoint_id_1));
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_context_helper_normal_leave()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_context_leave(nanoepoch_2, stream_id_1, checkpoint_id_1);

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            neutrino::helpers::context_t ctx(stream_id_1, checkpoint_id_1, nanoepoch_1, nanoepoch_2);
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_context_helper_exception()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_context_panic(nanoepoch_2, stream_id_1, checkpoint_id_1);

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            try
            {
                neutrino::helpers::context_t ctx(stream_id_1, checkpoint_id_1, nanoepoch_1, nanoepoch_2);
                throw "leave context with exception";
            }
            catch (...)
            {
            }
        }
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_context_helper_exception_and_normal_interleaved()
    {
        SCOPED_TRACE(__FUNCTION__);
        (*m_mock_consumer)
            .expect_context_enter(nanoepoch_1, stream_id_1, checkpoint_id_1)
            .expect_context_enter(nanoepoch_2, stream_id_1, checkpoint_id_2)
            .expect_context_panic(nanoepoch_3, stream_id_1, checkpoint_id_2)
            .expect_context_leave(nanoepoch_4, stream_id_1, checkpoint_id_1);

        channel_guard_t<transport_encoding> g(*m_mock_consumer);

        {
            neutrino::mock::scoped_guard sg(g.m_channel->m_consumer_stub);

            neutrino::helpers::context_t ctx(stream_id_1, checkpoint_id_1, nanoepoch_1, nanoepoch_4);
            try
            {
                neutrino::helpers::context_t ctx(stream_id_1, checkpoint_id_2, nanoepoch_2, nanoepoch_3);
                throw "leave context with exception";
            }
            catch (...)
            {
            }
        }
    }
};

TEST_F(neutrino_general_workflow_tests, checkpoint_same_stream)
{
    validate_checkpoint_same_stream<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_checkpoint_same_stream<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_checkpoint_same_stream<transport::frame_v00::known_encodings_t::JSON>();
}

TEST_F(neutrino_general_workflow_tests, checkpoint_diff_stream)
{
    validate_checkpoint_different_stream<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_checkpoint_different_stream<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_checkpoint_different_stream<transport::frame_v00::known_encodings_t::JSON>();
}

TEST_F(neutrino_general_workflow_tests, context_enter_leave_same_stream)
{
    validate_context_enter_leave_same_stream<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_context_enter_leave_same_stream<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_context_enter_leave_same_stream<transport::frame_v00::known_encodings_t::JSON>();
}

TEST_F(neutrino_general_workflow_tests, context_enter_leave_interleaved_stream)
{
    validate_context_enter_leave_interleaved_stream<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_context_enter_leave_interleaved_stream<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_context_enter_leave_interleaved_stream<transport::frame_v00::known_encodings_t::JSON>();
}

TEST_F(neutrino_general_workflow_tests, context_enter_panic_same_stream)
{
    validate_context_enter_panic_same_stream<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_context_enter_panic_same_stream<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_context_enter_panic_same_stream<transport::frame_v00::known_encodings_t::JSON>();
}

TEST_F(neutrino_general_workflow_tests, context_enter_panic_interleaved_stream)
{
    validate_context_enter_panic_interleaved_stream<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_context_enter_panic_interleaved_stream<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_context_enter_panic_interleaved_stream<transport::frame_v00::known_encodings_t::JSON>();
}

TEST_F(neutrino_general_workflow_tests, context_helper_normal_leave)
{
    validate_context_helper_normal_leave<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_context_helper_normal_leave<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_context_helper_normal_leave<transport::frame_v00::known_encodings_t::JSON>();
}
TEST_F(neutrino_general_workflow_tests, context_helper_exception)
{
    validate_context_helper_exception<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_context_helper_exception<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_context_helper_exception<transport::frame_v00::known_encodings_t::JSON>();
}
TEST_F(neutrino_general_workflow_tests, context_helper_exception_and_normal_interleaved)
{
    validate_context_helper_exception_and_normal_interleaved<transport::frame_v00::known_encodings_t::BINARY_NATIVE>();
    validate_context_helper_exception_and_normal_interleaved<transport::frame_v00::known_encodings_t::BINARY_NETWORK>();
    //validate_context_helper_exception_and_normal_interleaved<transport::frame_v00::known_encodings_t::JSON>();
}
*/