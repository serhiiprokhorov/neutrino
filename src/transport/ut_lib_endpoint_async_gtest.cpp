#include <gtest/gtest.h>
#include <thread>
#include <neutrino_mock.hpp>

#include <random>

#include <neutrino_transport_endpoint_async_win32.hpp>

using namespace neutrino::impl;

/*
            struct async_client_endpoint_t : public endpoint_t, public worker_t
            {
                async_client_endpoint_t(const async_connection_monitor_params_t asp)
                    : worker_t(asp), endpoint_t()
                {
                }

                void run_until(std::function<bool()>) override;
            };

            struct async_server_endpoint_t : public server_endpoint_t, public worker_t
            {
                async_server_endpoint_t(const async_connection_monitor_params_t asp, endpoint_impl_t& endpoint_impl)
                    : worker_t(asp), server_endpoint_t(endpoint_impl)
                {
                }

                void run_until(std::function<bool()>) override;
            };

*/

namespace
{
    const static auto test_buffers = []() 
    {
        std::vector<std::vector<uint8_t>> ret;
        for (uint8_t i = 1; i < (uint8_t)0xff; i++)
        {
            ret.emplace_back(i, i);
        }
        return ret;
    }();
}


struct neutrino_async_endpoints_tests : public ::testing::Test
{
    std::shared_ptr<transport::endpoint_t> m_frames_collector;

    struct run_parameters_t
    {
        neutrino::impl::transport::async_client_endpoint_t* m_client = nullptr;
        neutrino::impl::transport::async_server_endpoint_t* m_server = nullptr;
        std::size_t cc_frames = 0;
        std::atomic<std::size_t> bytes_sent = 0;
        std::atomic<std::size_t> bytes_failed = 0;
    };

    static void run_client(run_parameters_t& p)
    {
        const std::size_t retries = 100;
        for(std::size_t cc = 0; cc < p.cc_frames; cc++)
        {
            const auto& b = test_buffers[std::rand() % test_buffers.size()];

            auto cc_retries = retries;
            do
            {
                if (p.m_client->consume(&(b[0]), &(b[b.size() - 1]) + 1 /*past last item*/))
                {
                    p.bytes_sent += b.size();
                    break;
                }
            }
            while(cc_retries--);

            if(!cc_retries)
            {
                // TODO: collect failed attempts
                p.bytes_failed += b.size();
            }
            
        }

        // perform flush() by calling with 0-len data 
        uint8_t dummy[1];
        p.m_client->consume(dummy, dummy);
    }

    void validate_run(const run_parameters_t& p)
    {
        char msg_buf[1000];
        ASSERT_TRUE(m_frames_collector);

        {
            auto& cast_m_frames_collector = static_cast<neutrino::mock::frames_collector_t&>(*m_frames_collector);
            ASSERT_FALSE(cast_m_frames_collector.m_sumbissions.empty());

            snprintf(msg_buf, sizeof(msg_buf) - 1, "total frames %d", cast_m_frames_collector.m_sumbissions.size());
            SCOPED_TRACE(msg_buf);

            std::size_t submission_cc = 0;
            std::size_t bytes_received = 0;
            for(const auto& submission : cast_m_frames_collector.m_sumbissions)
            {
                bytes_received += submission.m_buffer.size();
                snprintf(msg_buf, sizeof(msg_buf) - 1, "submission # %d size %d", submission_cc++, submission.m_buffer.size());
                SCOPED_TRACE(msg_buf);
                ASSERT_TRUE(submission.m_buffer.size() <= po.m_params.m_message_buf_size) << "individual submission can't exceed expected buffer size";

                // frames are jammed into one buffer, validate frame by frame
                std::size_t frame_start_idx = 0;
                std::size_t frame_cc = 0;
                do
                {
                    // frame contains same symbol A repeated A times 
                    // any other symbol in a region [frme_start ... frame_start + A] means the buffer is corrupted 
                    auto expected_frame_symbol = submission.m_buffer[frame_start_idx];
                    auto expected_frame_end = frame_start_idx + expected_frame_symbol;

                    snprintf(msg_buf, sizeof(msg_buf) - 1, "frame # %d symbol %d", frame_cc, expected_frame_symbol);
                    SCOPED_TRACE(msg_buf);

                    ASSERT_TRUE(expected_frame_end > frame_start_idx); // self check
                    if (expected_frame_end > submission.m_buffer.size())
                    {
                        ASSERT_TRUE(expected_frame_end <= submission.m_buffer.size())
                        << "frame ends at " << expected_frame_end << " outside of submission size " << submission.m_buffer.size();
                    }

                    std::size_t cc_symbol_in_frame = 0;
                    while(++frame_start_idx < expected_frame_end)
                    {
                        if(expected_frame_symbol != submission.m_buffer[frame_start_idx])
                        {
                            ASSERT_EQ(expected_frame_symbol, submission.m_buffer[frame_start_idx]) << "frame is corrupted at idx " << frame_start_idx << " symbol # " << cc_symbol_in_frame << " countdown to frame end " << (expected_frame_end - frame_start_idx);
                        }
                        ASSERT_EQ(expected_frame_symbol, submission.m_buffer[frame_start_idx]) << "frame is corrupted at idx " << frame_start_idx << " symbol # " << cc_symbol_in_frame << " countdown to frame end " << (expected_frame_end - frame_start_idx);
                        cc_symbol_in_frame++;
                    }

                    frame_cc++;
                }
                while(frame_start_idx < submission.m_buffer.size());
            }
            ASSERT_EQ(p.bytes_sent.load(), bytes_received) << "bytes sent/received mismatch";
            ASSERT_EQ(p.bytes_failed, 0);
            
            cast_m_frames_collector.m_sumbissions.clear();
        }
    }

    void SetUp() final
    {
        std::srand(std::time(0));
        m_frames_collector.reset(new neutrino::mock::frames_collector_t());
    }

    void TearDown() final
    {
        auto& cast_m_frames_collector = static_cast<neutrino::mock::frames_collector_t&>(*m_frames_collector);
        ASSERT_TRUE(cast_m_frames_collector.m_sumbissions.empty());
        m_frames_collector.reset();
    }

    void run_win32_HANDLE_transport(
        run_parameters_t& p
    )
    {
        std::atomic<bool> transmission_complete = false;
        
        p.m_server.serve_until([&transmission_complete]() { return transmission_complete.load(); });

        //TODO: synchronize server thread!!
        run_client(p);

        transmission_complete = true;
        p.m_server.join();
    }
};

TEST_F(neutrino_async_endpoints_tests, win32_pipe)
{
    const std::string pipe_name{"\\\\.\\pipe\\neutrino_async_endpoints_tests"};

    neutrino::impl::transport::worker_t::async_connection_monitor_params_t apo;
    apo.m_connection_retry_timeout = std::chrono::milliseconds{1000};
    apo.m_connection_timeout = std::chrono::milliseconds{100};

    // TODO: add options : smaller than transmission, bigger than transmission
    DWORD nOutBufferSize = 10000;
    DWORD nInBufferSize = 10000;

    neutrino::impl::transport::namedpipe_win32_HANDLE_async_client_endpoint_t client(apo, pipe_name);

    neutrino::impl::transport::namedpipe_win32_HANDLE_async_server_endpoint_t server(
        apo
        , pipe_name
        , nOutBufferSize
        , nInBufferSize
        , *m_frames_collector
    );

    run_parameters_t p;
    p.m_client = &client;
    p.m_server = &server;
    p.cc_frames = 10000;

    run_win32_HANDLE_transport(p);
    validate_run(p);
}