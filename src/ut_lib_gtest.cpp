#include <gtest/gtest.h>
#include <thread>
//#include <neutrino_mock.hpp>

#include <neutrino_producer.hpp>

#include <neutrino_transport_buffered_endpoint_proxy_mt.hpp>
#include <neutrino_transport_buffered_endpoint_proxy_st.hpp>

using namespace neutrino::impl;

/*
namespace
{
    const uint64_t checkpoint_id_1 = 1;
    const uint64_t checkpoint_id_2 = 2;
    const uint64_t checkpoint_id_3 = 3;
    const uint64_t checkpoint_id_4 = 4;

    const uint64_t context_id_1 = 3;
    const uint64_t context_id_2 = 4;

    const uint64_t stream_id_1 = 301;
    const uint64_t stream_id_2 = 302;

    struct named_buffered_endpoint_params_t
    {
        transport::buffered_endpoint_proxy_t::buffered_endpoint_params_t m_params;
        std::string m_name;
    };

    const static auto test_params = []()
    {
        return std::vector<named_buffered_endpoint_params_t>{
            { {1000, 1}, "1000/1"}
            , { {1000, 500}, "1000/500" }
                , { {1000, 750}, "1000/750" }
                , { {1000, 999}, "1000/999" }
                , { {1000, 1000}, "1000/1000" }
                , { {1000, 1001}, "1000/1001" }
                , { {1000, 1100}, "1000/1100" }
        };
    }();

    struct named_buffered_optimistic_endpoint_params_t
    {
        transport::buffered_optimistic_endpoint_t::buffered_optimistic_consumer_params_t m_params;
        std::string m_name;
    };

    const static auto test_optimistic_params = []()
    {
        return std::vector<named_buffered_optimistic_endpoint_params_t>{
            { 1, "1"}
            , { {100}, "100" }
            , { {1000}, "1000" }
        };
    }();
}

using namespace neutrino::impl;

struct neutrino_general_with_transport_workflow_tests : public ::testing::Test
{
    struct dummy_exception
    {
    };

    static void run_transaction(neutrino::impl::local::payload::stream_id_t::type_t stream_id)
    {
        neutrino_checkpoint(neutrino_nanoepoch(), stream_id, checkpoint_id_1);
        {
            neutrino::helpers::context_t ctx(stream_id, context_id_1);
            try
            {
                neutrino::helpers::context_t ctx(stream_id, context_id_2);
                throw dummy_exception{};
            }
            catch(const dummy_exception&)
            {
                neutrino_checkpoint(neutrino_nanoepoch(), stream_id, checkpoint_id_2);
            }
            neutrino_checkpoint(neutrino_nanoepoch(), stream_id, checkpoint_id_3);
        }
        neutrino_checkpoint(neutrino_nanoepoch(), stream_id, checkpoint_id_4);
    }

    typedef std::function<std::shared_ptr<transport::endpoint_t>(std::shared_ptr<transport::endpoint_t>)> buffered_endpoint_factory_t;

    template <transport::frame_v00::known_encodings_t transport_encoding>
    struct channel_t
    {
        std::shared_ptr<transport::endpoint_impl_t> m_endpoint_impl;

        std::shared_ptr<transport::endpoint_t> m_endpoint;

        std::shared_ptr<neutrino::mock::connection_t<neutrino::impl::transport::endpoint_impl_t>> m_connection;
        std::shared_ptr<transport::consumer_stub_t> m_consumer_stub;

        std::shared_ptr<neutrino::impl::transport::consumer_stub_t> m_prev_consumer;

        channel_t(buffered_endpoint_factory_t buffered_endpoint_factory, neutrino::mock::consumer_t& consumer)
        {
            // deserialises from raw and forwards frames to consumer
            m_endpoint_impl = transport::frame_v00::create_endpoint_impl(transport_encoding, consumer);

            // collects raw frames and forwards to endpoint impl
            m_connection.reset(new neutrino::mock::connection_t<neutrino::impl::transport::endpoint_impl_t>(*m_endpoint_impl));

            m_endpoint = buffered_endpoint_factory(m_connection);

            // serializes frames to raw and forwards to endpoint
            m_consumer_stub = transport::frame_v00::create_consumer_stub(
                transport_encoding, *m_endpoint);

            // istall consumer, API will call it
            m_prev_consumer = producer::set_consumer(m_consumer_stub);
        }
        ~channel_t()
        {
            // restore just in case
            producer::set_consumer(m_prev_consumer);
        }

    };

    std::shared_ptr<neutrino::mock::consumer_t> m_mock_consumer;

    void SetUp() final
    {
    }

    void TearDown() final
    {
    }

    template <transport::frame_v00::known_encodings_t transport_encoding>
    void validate_transaction_singlethread(std::function<std::shared_ptr<transport::endpoint_t>(std::shared_ptr<transport::endpoint_t> endpoint)>f)
    {
        SCOPED_TRACE(__FUNCTION__);
        // events sequence corresponds to run_transaction impl
        m_mock_consumer.reset(new neutrino::mock::consumer_t());
        m_mock_consumer->
            expect_checkpoint(0, stream_id_1, checkpoint_id_1)
            .expect_context_enter(0, stream_id_1, context_id_1)
            .expect_context_enter(0, stream_id_1, context_id_2)
            .expect_context_panic(0, stream_id_1, context_id_2)
            .expect_checkpoint(0, stream_id_1, checkpoint_id_2)
            .expect_checkpoint(0, stream_id_1, checkpoint_id_3)
            .expect_context_leave(0, stream_id_1, context_id_1)
            .expect_checkpoint(0, stream_id_1, checkpoint_id_4)
            ;

        channel_t<transport_encoding> channel(f, *m_mock_consumer);

        try
        {
            run_transaction(stream_id_1);

            // expect flush at the very end of simulated app operations 
            neutrino_flush();
        }
        catch(const std::exception& e)
        {
            ADD_FAILURE() << e.what();
        }

        ASSERT_EQ(std::size_t{0}, m_mock_consumer->m_expected_checkpoints.size());
        ASSERT_EQ(std::size_t{0}, m_mock_consumer->m_expected_contexts.size());
        m_mock_consumer.reset();
    }

    void validate_transaction_singlethread_transport_options(std::function<std::shared_ptr<transport::endpoint_t>(std::shared_ptr<transport::endpoint_t>)>f)
    {
        SCOPED_TRACE(__FUNCTION__);

        validate_transaction_singlethread<transport::frame_v00::known_encodings_t::BINARY_NATIVE>(f);
        validate_transaction_singlethread<transport::frame_v00::known_encodings_t::BINARY_NETWORK>(f);
        //validate_transaction_singlethread<transport::frame_v00::known_encodings_t::JSON>(ep_factory);
    }
};

TEST_F(neutrino_general_with_transport_workflow_tests, transaction_singlethread)
{
    for(const auto& po : test_params)
    {
        SCOPED_TRACE(po.m_name);
        {
            SCOPED_TRACE("buffered_singlethread_endpoint_t");
            validate_transaction_singlethread_transport_options(
                [po](std::shared_ptr<transport::endpoint_t> chained_endpoint)
                {
                    return std::shared_ptr<transport::endpoint_t>(new transport::buffered_singlethread_endpoint_t(chained_endpoint, po.m_params));
                }
            );
        }
        {
            SCOPED_TRACE("buffered_exclusive_endpoint_t");
            validate_transaction_singlethread_transport_options(
                [po](std::shared_ptr<transport::endpoint_t> chained_endpoint)
                {
                    return std::shared_ptr<transport::endpoint_t>(new transport::buffered_exclusive_endpoint_t(chained_endpoint, po.m_params));
                }
            );
        }
        {
            SCOPED_TRACE("buffered_optimistic_endpoint_t");
            for (const auto& opo : test_optimistic_params)
            {
                SCOPED_TRACE(opo.m_name);
                validate_transaction_singlethread_transport_options(
                    [opo, po](std::shared_ptr<transport::endpoint_t> chained_endpoint)
                    {
                        return std::shared_ptr<transport::endpoint_t>(new transport::buffered_optimistic_endpoint_t(chained_endpoint, po.m_params, opo.m_params));
                    }
                );
            }
        }
    }
}
*/