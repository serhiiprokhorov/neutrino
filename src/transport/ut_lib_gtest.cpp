#include <gtest/gtest.h>
#include <thread>
#include <neutrino_mock.hpp>

#include <random>

#include <neutrino_transport_buffered_st.hpp>
#include <neutrino_transport_buffered_mt.hpp>

using namespace neutrino::impl;

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

    struct named_buffered_endpoint_params_t
    {
        transport::buffered_endpoint_t::buffered_endpoint_params_t m_params;
        std::string m_name;
    };

    const static auto test_params = []()
    {
        return std::vector<named_buffered_endpoint_params_t>{
            { {1000, 1}, "1000/1"}
            , { {1000, 500}, "1000/500"}
            , { {1000, 750}, "1000/750" }
            , { {1000, 999}, "1000/999" }
            , { {1000, 1000}, "1000/1000" }
            , { {1000, 1001}, "1000/1001" }
            , { {1000, 1100}, "1000/1100" }
        };
    }();
}


struct neutrino_buffered_endpoints_tests : public ::testing::Test
{
    std::shared_ptr<transport::endpoint_t> m_frames_collector;

    struct run_parameters_t
    {
        transport::buffered_endpoint_t* e = nullptr;
        std::size_t cc_frames = 0;
        std::atomic<std::size_t> bytes_sent = 0;
        std::atomic<std::size_t> bytes_failed = 0;
    };

    static void run_iterations(run_parameters_t& p)
    {
        const std::size_t retries = 100;
        for(std::size_t cc = 0; cc < p.cc_frames; cc++)
        {
            const auto& b = test_buffers[std::rand() % test_buffers.size()];

            auto cc_retries = retries;
            do
            {
                if (p.e->consume(&(b[0]), &(b[b.size() - 1]) + 1 /*past last item*/))
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
        p.e->consume(dummy, dummy);
    }

    void validate_run(const run_parameters_t& p, const named_buffered_endpoint_params_t& po)
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

    void validate_buffered_singlethread()
    {
        for (const auto& po : test_params)
        {
            SCOPED_TRACE(po.m_name);
            transport::buffered_singlethread_endpoint_t e(m_frames_collector, po.m_params);
            run_parameters_t p{ &e, 100000 };
            run_iterations(p);
            validate_run(p, po);
        }
    }

    template <typename endpoint_factory>
    void validate_buffered_multithread(endpoint_factory f)
    {
        const std::size_t cc_threads = 20;
        for (const auto& po : test_params)
        {
            SCOPED_TRACE(po.m_name);
            auto e{f(po)};
            run_parameters_t p{ e.get(), 100000 / cc_threads };

            std::mutex m;
            std::condition_variable v;
            bool ready = false;

            auto l = [&v, &m, &ready, &p]()
            {
                {
                    std::unique_lock<std::mutex> lk(m);
                    v.wait(lk, [&ready] {return ready; });
                    lk.unlock();
                }
                run_iterations(p);
            };

            std::list<std::thread> tt;

            for (std::size_t cc = 0; cc < cc_threads; cc++)
                tt.emplace_back(l);

            {
                std::lock_guard<std::mutex> lk(m);
                ready = true;
            }
            v.notify_all();

            for (auto& t : tt)
                t.join();

            validate_run(p, po);
        }
    }
};

TEST_F(neutrino_buffered_endpoints_tests, buffered_st)
{
    validate_buffered_singlethread();
}

TEST_F(neutrino_buffered_endpoints_tests, buffered_mt_exclusive)
{
    validate_buffered_multithread(
        [this](const auto& po)
        {
            return std::make_shared<transport::buffered_exclusive_endpoint_t>(m_frames_collector, po.m_params);
        }
    );
}

TEST_F(neutrino_buffered_endpoints_tests, buffered_mt_optimistic_default_optimistic_retry)
{
    transport::buffered_optimistic_endpoint_t::buffered_optimistic_consumer_params_t opo;

    validate_buffered_multithread(
        [this, &opo](const auto& po)
        {
            return std::make_shared<transport::buffered_optimistic_endpoint_t>(m_frames_collector, po.m_params, opo);
        }
    );
}

#if (USE_MT)
TEST_F(neutrino_buffered_endpoints_tests, buffered_mt)
{
    ADD_FAILURE();
}
#endif