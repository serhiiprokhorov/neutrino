#include <gtest/gtest.h>

#include <future>

#include <neutrino_transport_shared_mem_endpoint_proxy_st.hpp>
#include <neutrino_transport_shared_mem_endpoint_proxy_mt.hpp>
#include <neutrino_transport_shared_mem_win.hpp>

const std::size_t num_buffers = 5;
const DWORD buf_size = 1000;

std::vector<uint8_t> generate_expected_data()
{
  // expeted_data is a bytestream to be sent by endproint proxy to a listener
  // it is build of stripes of random values
  // on singlethread tests: a received bytestream is just compared to expected_data
  // on multithreaded tests: can't just compare bytestream because of threads randomness 
  //    but the blocks of expeted_data must not be interleaved 
  std::vector<std::vector<std::vector<uint8_t>>> blocks0;
  blocks0.reserve(0xff);

  std::size_t total_generated = 0; // approx 1Mb
  for (int i = 0; i < 0xff; i++)
  {
    blocks0.push_back({});

    auto& blocks1 = blocks0.back();
    blocks1.reserve(0xff);

    for (int y = 0; y < 0xff; y++)
    {
      blocks1.push_back({});
      auto& blocks2 = blocks1.back();

      const auto block_size = std::rand() % 20;

      blocks2.reserve(block_size);
      total_generated += block_size;

      for (std::size_t z = 0; z < block_size; z++)
        blocks2.push_back(std::rand() % 0xff);
    }
  }

  const auto sz = 100 * total_generated /*a decent number to ensure no blocks are interleaved in both ST and MT envs*/;
  std::vector<uint8_t> ret;
  ret.reserve(sz);

  while (ret.size() < sz)
  {
    uint8_t b0 = std::rand() % blocks0.size();
    uint8_t b1 = std::rand() % blocks0[b0].size();

    auto& stripe = blocks0[b0][b1];

    if (ret.size() + stripe.size() > sz)
      break;

    ret.push_back(b0);
    ret.push_back(b1);
    for (const auto& x : blocks0[b0][b1])
    {
      ret.emplace_back(x);
    }
  }

  return ret;
};

const static std::vector<uint8_t> expected_data = generate_expected_data();

class neutrino_transport_shared_mem_endpoint_proxy_Fixture : public ::testing::Test {
  public:

  std::unique_ptr<neutrino::impl::memory::win_shared_mem::v00_names_t> nm;

  std::shared_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > host_pool;

  std::shared_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > guest_pool;

  void generate_pools()
  {
    nm.reset(new neutrino::impl::memory::win_shared_mem::v00_names_t(123, "domain", "suffix"));

    host_pool.reset(new neutrino::impl::memory::win_shared_mem::v00_pool_t(
      num_buffers
      , neutrino::impl::memory::win_shared_mem::OPEN_MODE::CREATE
      , *nm
      , buf_size));

    guest_pool.reset(new neutrino::impl::memory::win_shared_mem::v00_pool_t(
      num_buffers
      , neutrino::impl::memory::win_shared_mem::OPEN_MODE::OPEN
      , *nm
      , buf_size));

    ASSERT_TRUE(guest_pool->m_buffer.load()->is_clean());
    ASSERT_TRUE(host_pool->m_buffer.load()->is_clean());
  }

  void SetUp() override {

    generate_pools();
    
  }
};

class singlethreaded_producer : public neutrino_transport_shared_mem_endpoint_proxy_Fixture {
public:
  void validate_producer(neutrino::impl::transport::endpoint_proxy_t& x)
  {
    neutrino::impl::memory::win_shared_mem::v00_async_listener_t listener(host_pool);

    std::atomic<std::ptrdiff_t> actual_consumed_cc = 0;
    std::vector<uint8_t> consumed_list;
    consumed_list.resize(expected_data.size());
    uint8_t* pStart = &(consumed_list[0]);
    uint8_t* pData = pStart;

    uint64_t sequence_mismatch = 0;

    const uint8_t* pX = &(expected_data[0]);
    // listener.start returns a functor which, when called, instructs listener to exit
    auto at_cancel = listener.start(
      [this, &sequence_mismatch, &consumed_list, &actual_consumed_cc, &pData, &pStart, &pX](const uint8_t* p, const uint8_t* e)
      {
        // this function is called when the listener gets another portion of data
        const auto bytes = e - p;
        memmove(pData, p, bytes);
        pData += bytes;
        actual_consumed_cc += bytes;

        for (std::size_t x = 0; x < bytes; x++)
        {
          if (pX[x] != p[x])
          {
            sequence_mismatch++;
          }
        }

        pX += bytes;
      }
    );

    auto f = std::async(
      std::launch::async,
      [this, &x, &actual_consumed_cc]()
      {
        std::ptrdiff_t produced_cc = 0;
        for (; produced_cc < expected_data.size() - 1; produced_cc++)
        {
          EXPECT_TRUE(x.consume(&(expected_data[produced_cc]), &(expected_data[produced_cc + 1]))) << "consumee failure at " << produced_cc;
        }
        EXPECT_TRUE(x.consume(&(expected_data[0]), &(expected_data[0]))) << "flush failure";

        return produced_cc;
      }
    );

    auto timedout = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 60000 };
    ASSERT_TRUE(f.wait_until(timedout) == std::future_status::ready);
    auto actual_transmitted_cc = f.get();

    while (std::chrono::steady_clock::now() < timedout)
    {
      if (actual_consumed_cc.load() >= actual_transmitted_cc)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
    }

    ASSERT_EQ(0, sequence_mismatch);

    {
      const auto actual_consumed = actual_consumed_cc.load();
      ASSERT_EQ(actual_consumed, actual_transmitted_cc);

      for (std::size_t i = 0; i < actual_consumed; i++)
      {
        ASSERT_EQ(consumed_list[i], expected_data[i]) << i;
      }
    }

    at_cancel();
  }
};

TEST_F(singlethreaded_producer, st_singlethread_shared_memory_endpoint_proxy_t)
{
  // this test ensures singlethread producer and any of endpoint proxies are capable to transfer data when paired with win_shared_mem::v00_async_listener_t
  // transfers data with no corruptions, no missed bbytes, no overlaps

  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t x(params, guest_pool);
    
  validate_producer(x);
}

TEST_F(singlethreaded_producer, st_exclusive_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_producer(x);
}


TEST_F(singlethreaded_producer, st_optimistic_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t x(
      params
      , guest_pool
      , neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::optimistic_mt_shared_memory_endpoint_proxy_params_t()
    );

  validate_producer(x);
}

class mutilthreaded_producer : public neutrino_transport_shared_mem_endpoint_proxy_Fixture {
public:
  void validate_producer(neutrino::impl::transport::endpoint_proxy_t& x)
  {
    ASSERT_TRUE(false) << "not implemented"; 
    neutrino::impl::memory::win_shared_mem::v00_async_listener_t listener(host_pool);

    std::atomic<std::ptrdiff_t> actual_consumed_cc = 0;
    std::vector<uint8_t> consumed_list;
    consumed_list.resize(expected_data.size());
    uint8_t* pStart = &(consumed_list[0]);
    uint8_t* pData = pStart;

    uint64_t sequence_mismatch = 0;

    const uint8_t* pX = &(expected_data[0]);
    auto at_cancel = listener.start(
      [this, &sequence_mismatch, &consumed_list, &actual_consumed_cc, &pData, &pStart, &pX](const uint8_t* p, const uint8_t* e)
      {
        const auto bytes = e - p;
        memmove(pData, p, bytes);
        pData += bytes;
        actual_consumed_cc += bytes;

        for (std::size_t x = 0; x < bytes; x++)
        {
          if (pX[x] != p[x])
          {
            sequence_mismatch++;
          }
        }

        pX += bytes;
      }
    );

    std::promise<void> promise_start_all;
    auto shared_future_start_all = promise_start_all.get_future().share();

    std::future<std::ptrdiff_t> futures[10];
    const std::size_t num_threads = sizeof(futures) / sizeof(futures[0]);
    std::ptrdiff_t actual_transmitted_cc = 0;

    for (std::size_t t = 0; t < num_threads; t++)
    {
      SCOPED_TRACE(std::to_string(t));
      futures[t] = std::async(
        std::launch::async,
        [this, &x, &actual_consumed_cc, &shared_future_start_all]()
        {
          // barrier-like code to sync multiple producers
          shared_future_start_all.wait();
          shared_future_start_all.get();

          std::ptrdiff_t produced_cc = 0;
          for (; produced_cc < expected_data.size() - 1; produced_cc++)
          {
            EXPECT_TRUE(x.consume(&(expected_data[produced_cc]), &(expected_data[produced_cc + 1]))) << "consumee failure at " << produced_cc;
          }
          EXPECT_TRUE(x.consume(&(expected_data[0]), &(expected_data[0]))) << "flush failure";

          return produced_cc;
        }
      );
    }

    promise_start_all.set_value();

    const auto timedout = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 60000 };

    std::size_t still_waiting = 0;
    while (timedout > std::chrono::steady_clock::now())
    {
      still_waiting = 0;
      for (auto& f : futures)
      {
        if (!f.valid())
          continue;

        const auto fr = f.wait_for(std::chrono::microseconds{ 1 });
        if (fr == std::future_status::timeout)
        {
          still_waiting++;
        }
        else
          if (fr == std::future_status::ready)
          {
            actual_transmitted_cc += f.get();
          }
      }
      if (!still_waiting)
        break;
    }

    ASSERT_EQ(0, still_waiting) << "timeout";

    EXPECT_EQ(0, sequence_mismatch);

    {
      const auto actual_consumed = actual_consumed_cc.load();
      ASSERT_EQ(actual_consumed, actual_transmitted_cc);

      for (std::size_t i = 0; i < actual_consumed; i++)
      {
        ASSERT_EQ(consumed_list[i], expected_data[i]) << i;
      }
    }

    at_cancel();
  }
};

TEST_F(mutilthreaded_producer, exclusive_mt_shared_memory_endpoint_proxy_t)
{
  ASSERT_TRUE(false) << "not implemented";
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_producer(x);
}


TEST_F(mutilthreaded_producer, optimistic_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t x(
      params
      , guest_pool
      , neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::optimistic_mt_shared_memory_endpoint_proxy_params_t()
    );

  validate_producer(x);
}
