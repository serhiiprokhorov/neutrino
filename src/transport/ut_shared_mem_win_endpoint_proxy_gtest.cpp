#include <gtest/gtest.h>

#include <future>
#include <array>

#include <neutrino_transport_shared_mem_endpoint_proxy_st.hpp>
#include <neutrino_transport_shared_mem_endpoint_proxy_mt.hpp>
#include <neutrino_transport_shared_mem_win.hpp>

const std::size_t num_buffers = 5;
const DWORD buf_size = 1000;

static std::vector<std::vector<uint8_t>> expected_data = []() -> auto {
  // every hi-byte is a thread id (0..0xf)
  // every low-byte is a number (0..0xf)
  const uint8_t num_threads = 0x0f;

  std::vector<std::vector<uint8_t>> ret(num_threads, std::vector<uint8_t>(size_t{ 100000000 }));

  size_t x = 0;
  std::generate_n(ret[0].begin(), ret[0].size(), [&x]() -> uint8_t { return (x++) & 0xf; });

  for (uint8_t i = 0; i < num_threads; i++) {
    ret[i] = ret[0];
    std::for_each(ret[i].begin(), ret[i].end(), [thread_id = i << 4](auto& v) { v |= thread_id;  });
  }

  return ret;
}();

template <typename It, std::size_t num_bytes_per_single_consume_op> 
struct bytes_to_consume 
{
  inline std::ptrdiff_t get(It s, It e) {
    auto ret = e - s;
    return ret > num_bytes_per_single_consume_op ? num_bytes_per_single_consume_op : ret;
  }
};

template <typename It>
struct bytes_to_consume<It, 0> {};

template <typename It>
struct bytes_to_consume<It, 1>
{
  inline std::ptrdiff_t get(It s, It e) {
    return s >= e ? 0 : 1;
  }
};

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

  template <std::size_t num_bytes_per_single_consume_op>
  void validate_data_transfer(neutrino::impl::transport::endpoint_proxy_t& x, const size_t num_threads) {

    ASSERT_TRUE(num_threads < expected_data.size());

    neutrino::impl::memory::win_shared_mem::v00_async_listener_t listener(host_pool);

    std::atomic<std::ptrdiff_t> actual_consumed_cc = 0;

    std::vector<std::vector<uint8_t>> consumed_list;
    consumed_list.resize(num_threads);

    for (std::size_t ii = 0; ii < num_threads; ii++) {
      consumed_list[ii].reserve(expected_data[ii].size());
    }

    // listener.start returns a functor which, when called, instructs listener to exit
    auto at_cancel = listener.start(
      [this, &consumed_list](uint64_t sequence, const uint8_t* p, const uint8_t* e)
      {

        // this function is called when the listener gets another portion of data
        // mutiple threads are sending data, each byte is encoded 0xAABB where 0x00AA is a thread id (0..consumed_list.size()) and BB is a some number of a range 0..0x00ff

        const auto bytes = e - p;
        while (p < e) {
          const auto thread_id = (*p) >> 4;
          if (thread_id > consumed_list.size())
            throw std::runtime_error(std::string("bad thread id ").append(std::to_string(thread_id)));
          consumed_list[thread_id].push_back(*p);
          p++;
        }
      }
    );


    // all threads are blocked at the beginning until promise_start_all is fired,
    // this guarantees all threads are sending data simultaneously (depends on how OS schedules those threads)
    std::promise<void> promise_start_all;
    std::shared_future<void> shared_future_start_all(promise_start_all.get_future());
    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    // schedule threads
    for (std::size_t t = 0; t < num_threads; t++)
    {
      futures.emplace_back(std::async(
        std::launch::async,
        [this, &x, &shared_future_start_all, t]()
        {
          // barrier-like code to sync multiple producers
          shared_future_start_all.wait();
          shared_future_start_all.get();

          const uint8_t* s = &(expected_data[t][0]);
          const uint8_t* e = s + expected_data[t].size();

          bytes_to_consume<decltype(s), num_bytes_per_single_consume_op> bytes_calculator;

          do
          {
            if (const auto bytes = bytes_calculator.get(s, e)) {
              if (!x.consume(s, s + bytes)) {
                char buf[200];
                snprintf(buf, sizeof(buf) / sizeof(buf[0]), "consume failure, thread_id=%llu, at=%lld, bytes=%lld", t, (e - s), bytes);
                throw std::runtime_error(buf);
              }
              s += bytes;
            }
            else {
              break;
            }
          } while (true);

          if (!x.consume(s, s)) {
            char buf[200];
            snprintf(buf, sizeof(buf) / sizeof(buf[0]), "flush failure, thread_id=%llu", t);
            throw std::runtime_error(buf);
          }
        }
      ));
    }

    // que all the threads sheduled in above
    promise_start_all.set_value();

    // expect data transmissions and cancellation to finish withing this timeout
    const auto timedout_producer = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 120000 };

    // wait on futures to be satisfied
    size_t i = 0;
    for (auto& f : futures)
    {
      std::cerr << i << " wait" << std::endl;
      const auto status = f.wait_until(timedout_producer);
      EXPECT_TRUE(status == std::future_status::ready) << "timeout thread " << i;
      f.get();
      i++;
    }

    // execute cancel in a separate thread to watch a timeout
    auto cancelled = std::async(std::launch::async, [&at_cancel]() { at_cancel(); });
    const auto status = cancelled.wait_until(std::chrono::steady_clock::now() + std::chrono::milliseconds{ 30000 });
    EXPECT_TRUE(status == std::future_status::ready) << "timeout cancelled ";

    // validate consumed_list contains the same data as expected_data
    for (std::size_t thread_id = 0; thread_id < futures.size(); thread_id++) {
      ASSERT_EQ(consumed_list[thread_id].size(), expected_data[thread_id].size()) << " thread_id=" << thread_id;
      for (std::size_t idx = 0; idx < consumed_list[thread_id].size(); idx++) {
        ASSERT_EQ(consumed_list[thread_id][idx], expected_data[thread_id][idx]) << " thread_id=" << thread_id << " idx=" << idx;
      }
    }
  }
};

class singlethreaded_producer : public neutrino_transport_shared_mem_endpoint_proxy_Fixture {
public:
};

TEST_F(singlethreaded_producer, one_byte_consume_st_singlethread_shared_memory_endpoint_proxy_t)
{
  // this test ensures singlethread producer and any of endpoint proxies are capable to transfer data when paired with win_shared_mem::v00_async_listener_t
  // transfers data with no corruptions, no missed bbytes, no overlaps

  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t x(params, guest_pool);
    
  validate_data_transfer<1>(x, 1);
}

TEST_F(singlethreaded_producer, two_byte_consume_st_singlethread_shared_memory_endpoint_proxy_t)
{
  // this test ensures singlethread producer and any of endpoint proxies are capable to transfer data when paired with win_shared_mem::v00_async_listener_t
  // transfers data with no corruptions, no missed bbytes, no overlaps

  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<2>(x, 1);
}

TEST_F(singlethreaded_producer, three_byte_consume_st_singlethread_shared_memory_endpoint_proxy_t)
{
  // this test ensures singlethread producer and any of endpoint proxies are capable to transfer data when paired with win_shared_mem::v00_async_listener_t
  // transfers data with no corruptions, no missed bbytes, no overlaps

  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<3>(x, 1);
}

TEST_F(singlethreaded_producer, ten_byte_consume_st_singlethread_shared_memory_endpoint_proxy_t)
{
  // this test ensures singlethread producer and any of endpoint proxies are capable to transfer data when paired with win_shared_mem::v00_async_listener_t
  // transfers data with no corruptions, no missed bbytes, no overlaps

  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<10>(x, 1);
}

TEST_F(singlethreaded_producer, one_byte_consume_st_exclusive_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<1>(x, 1);
}

TEST_F(singlethreaded_producer, two_byte_consume_st_exclusive_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<2>(x, 1);
}

TEST_F(singlethreaded_producer, three_byte_consume_st_exclusive_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<3>(x, 1);
}

TEST_F(singlethreaded_producer, ten_byte_consume_st_exclusive_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<10>(x, 1);
}

TEST_F(singlethreaded_producer, one_byte_consume_st_optimistic_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t x(
      params
      , guest_pool
      , neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::optimistic_mt_shared_memory_endpoint_proxy_params_t()
    );

  validate_data_transfer<1>(x, 1);
}

TEST_F(singlethreaded_producer, two_byte_consume_st_optimistic_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t x(
    params
    , guest_pool
    , neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::optimistic_mt_shared_memory_endpoint_proxy_params_t()
  );

  validate_data_transfer<2>(x, 1);
}

TEST_F(singlethreaded_producer, three_byte_consume_st_optimistic_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t x(
    params
    , guest_pool
    , neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::optimistic_mt_shared_memory_endpoint_proxy_params_t()
  );

  validate_data_transfer<3>(x, 1);
}

TEST_F(singlethreaded_producer, ten_byte_consume_st_optimistic_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t x(
    params
    , guest_pool
    , neutrino::impl::transport::optimistic_mt_shared_memory_endpoint_proxy_t::optimistic_mt_shared_memory_endpoint_proxy_params_t()
  );

  validate_data_transfer<10>(x, 1);
}

class mutilthreaded_producer : public neutrino_transport_shared_mem_endpoint_proxy_Fixture {
public:
};

TEST_F(mutilthreaded_producer, exclusive_mt_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::exclusive_mt_shared_memory_endpoint_proxy_t x(params, guest_pool);

  validate_data_transfer<1>(x, 0xf);
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

  validate_data_transfer<1>(x, 0xf);
}
