#include <gtest/gtest.h>

#include <future>
#include <array>

#include <neutrino_transport_shared_mem_endpoint_proxy_st.hpp>
#include <neutrino_transport_shared_mem_endpoint_proxy_mt.hpp>
#include <neutrino_transport_shared_mem_win.hpp>

const std::size_t num_buffers = 5;
const DWORD buf_size = 1000;

struct single_byte_generated_data {
  std::vector<uint8_t> data; // generated data
  std::size_t bytes; // number of valid bytes in .data

  static single_byte_generated_data generate(const size_t bytes_to_generate, const size_t thread_id) {
    single_byte_generated_data ret;
    ret.bytes = bytes_to_generate;
    ret.data.resize(ret.bytes + buf_size, 0);

    const std::size_t hibyte = thread_id << 4;

    auto* p = &(ret.data[0]);
    for (size_t x = 0; x < ret.bytes; x++) {
      *p = (x & 0xf) | hibyte;
    }

    return ret;
  }
};

static const std::vector<single_byte_generated_data> expected_data = []() -> auto {
  // every hi-byte is a thread id (0..0xf)
  // every low-byte is a number (0..0xf)
  std::vector<single_byte_generated_data> ret;

  for (std::size_t thread_id = 0; thread_id < 0x0f; thread_id++) {
    ret.push_back(single_byte_generated_data::generate(size_t{ 2000 * buf_size }, thread_id));
  }

  return ret;
}();

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
    // Validate neutrino transport is able to transfer data from producer's endpoint proxy to consumer's endpoint.
    // Following neutrino's loic, producer sends data to one of consumer's endpoints via an endpoint proxy and neutrino transports every byte from an endpoint proxy to its matchin endpoint.
    // The procedure here validates data transfer between producer and consumer, ensures no bytes lost and the order is preserved.
    // 
    // Assume data goes from producer to consumer.
    // The array expected_data represents the data to be sent, per each thread.
    // 
    // One or more producer threads (producers, param num_threads) loops over expected_data[thread id] and call endpoint proxy "consume" method to initiate data transfer.
    // Data is consumed in N-byte blocks (template parameter num_bytes_per_single_consume_op).
    // Consumer procedure is singlethred, this test defines very simple consumer while it starts v00_async_listener_t. Consumer collects data into a consumed_list which then compared to expected_data. 
    // 
    // Once all producers are done transferring data, the main thread waits for a few seconds to allow data transfer to complete. 
    //
    // The test is failed if any difference in bytes order or number of bytes consumed was found.


    ASSERT_TRUE(num_threads < expected_data.size());

    neutrino::impl::memory::win_shared_mem::v00_async_listener_t listener(host_pool);

    std::atomic<std::ptrdiff_t> actual_consumed_cc = 0;

    std::vector<std::vector<uint8_t>> consumed_list;
    consumed_list.resize(num_threads);

    std::vector<std::size_t> expected_bytes_by_thread(expected_data.size(), 0);

    std::size_t expected_blocks_cc = 0;
    std::size_t expected_bytes_cc = 0;

    for (std::size_t ii = 0; ii < num_threads; ii++) {
      expected_bytes_by_thread[ii] = expected_data[ii].bytes - (expected_data[ii].bytes % num_bytes_per_single_consume_op);

      expected_bytes_cc += expected_bytes_by_thread[ii];

      expected_blocks_cc += expected_bytes_by_thread[ii] / buf_size + 1;

      consumed_list[ii] = std::vector<uint8_t>(expected_bytes_by_thread[ii], '0');
      consumed_list[ii].resize(0);
    }

    {
      char buf[200];
      snprintf(buf, sizeof(buf) / sizeof(buf[0]), "expect blocks %lld bytes %lld\n", expected_blocks_cc, expected_bytes_cc);
      std::cerr << buf;
    }

    std::vector<std::size_t> consumed_blocks;
    consumed_blocks.reserve(expected_blocks_cc);

    std::atomic<std::size_t> consumed_bytes_cc = 0;

    std::promise<void> promise_consumer_done;
    std::shared_future<void> future_consumer_done(promise_consumer_done.get_future());

    uint64_t prev_sequence = 0;

    // listener.start returns a functor which, when called, instructs listener to exit
    auto at_cancel = listener.start(
      [this, &consumed_blocks , &consumed_list, &consumed_bytes_cc, &promise_consumer_done, expected_blocks_cc, expected_bytes_cc, &prev_sequence](uint64_t sequence, const uint8_t* p, const uint8_t* e)
      {
        char buf[200];

        if (p == nullptr) {
          std::cerr << "timeout\n";
          return;
        }

        ASSERT_EQ(prev_sequence + 1, sequence) << "sequence mismatch";
        prev_sequence = sequence;

        ASSERT_TRUE(p <= e) << "bad block";

        // this function is called when the listener gets another portion of data
        // mutiple threads are sending data, each byte is encoded 0xAABB where 0x00AA is a thread id (0..consumed_list.size()) and BB is a some number of a range 0..0x00ff

        const auto bytes = e - p;
        if (bytes) {

          ASSERT_TRUE(consumed_bytes_cc < expected_bytes_cc) << "consumed_bytes_cc=" << consumed_bytes_cc << " expected_bytes_cc=" << expected_bytes_cc;

          consumed_blocks.push_back(bytes);
          snprintf(buf, sizeof(buf) / sizeof(buf[0]), "s %lld b %lld\n", sequence, bytes);
          std::cerr << buf;

          while (p < e) {
            const auto thread_id = (*p) >> 4;
            ASSERT_TRUE(thread_id < consumed_list.size()) << "bad thread id " << thread_id << " exceedes " << consumed_list.size();
            consumed_list[thread_id].push_back(*p);
            p++;
          }

          consumed_bytes_cc += bytes;

          if (consumed_bytes_cc >= expected_bytes_cc) {
            ASSERT_TRUE(consumed_bytes_cc <= expected_bytes_cc) << "consumed_bytes_cc " << consumed_bytes_cc << " exceedes expected_bytes_cc " << expected_bytes_cc;
            promise_consumer_done.set_value();
          }
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
        [this, &x, &shared_future_start_all, t, &expected_bytes_by_thread]()
        {
          char buf[200];

          // barrier-like code to sync multiple producers
          shared_future_start_all.wait();
          shared_future_start_all.get();

          const uint8_t* s = &(expected_data[t].data[0]);
          const uint8_t* last = s + expected_bytes_by_thread[t];

          snprintf(buf, sizeof(buf) / sizeof(buf[0]), "producer t %lld\n", t);
          std::cerr << buf;

          do
          {
            const uint8_t* b = s;
            s += num_bytes_per_single_consume_op;
            if (!x.consume(b, s)) {
              char buf[200];
              snprintf(buf, sizeof(buf) / sizeof(buf[0]), "consume failure, thread_id=%llu, at=%lld", t, (s - &(expected_data[t].data[0])));
              throw std::runtime_error(buf);
            }
          } while (s < last);

          snprintf(buf, sizeof(buf) / sizeof(buf[0]), "producer flush t %lld\n", t);
          std::cerr << buf;

          if (!x.consume(s, s)) {
            char buf[200];
            snprintf(buf, sizeof(buf) / sizeof(buf[0]), "flush failure, thread_id=%llu", t);
            throw std::runtime_error(buf);
          }

          snprintf(buf, sizeof(buf) / sizeof(buf[0]), "producer done t %lld\n", t);
          std::cerr << buf;
        }
      ));
    }


    const auto producer_timeout = std::chrono::seconds(120);
    const auto c1 = std::chrono::steady_clock::now();
    {
      // sync all producers and wait for them to finish transfering data
      // NOTE: size of futures array defines the number of producers

      // que all the threads sheduled in above
      promise_start_all.set_value();

      size_t i = 0;
      for (auto& f : futures)
      {
        std::cerr << i << " wait" << std::endl;
        const auto status = f.wait_until(c1 + producer_timeout);
        EXPECT_TRUE(status == std::future_status::ready) << "timeout thread " << i;
        f.get();
        i++;
      }
    }
    const auto producer_duration = (std::chrono::steady_clock::now() - c1) + std::chrono::seconds(2);
    
    // wait for consumer to report expected bytes have been consumed
    // assume upper limit of consumer's duration as producer's duration doubled
    const auto consumer_status = future_consumer_done.wait_for(producer_duration + producer_duration);
    EXPECT_TRUE(consumer_status == std::future_status::ready) << "timeout consumer";

    // cancel consumer's operation
    // the consumer runs its operation in a separate thread which may hang, so we need to control its timeout
    // do canclel as async and wait on its future with a timeout
    auto cancelled = std::async(std::launch::async, [&at_cancel]() { at_cancel(); });
    const auto consumer_cancelled_status = cancelled.wait_until(std::chrono::steady_clock::now() + std::chrono::milliseconds{ 30 });
    EXPECT_TRUE(consumer_cancelled_status == std::future_status::ready) << "timeout cancelled ";

    // at this point both producer and consumer have reported the data transfer is complete:
    // -- all producer threads are finished transferring data
    // -- consumer has reported the expected number of bytes have been received
    // Below code validates the order of bytes per thread
    for (std::size_t thread_id = 0; thread_id < futures.size(); thread_id++) {
      ASSERT_EQ(consumed_list[thread_id].size(), expected_bytes_by_thread[thread_id]) << " thread_id=" << thread_id;
      for (std::size_t idx = 0; idx < consumed_list[thread_id].size(); idx++) {
        ASSERT_EQ(consumed_list[thread_id][idx], expected_data[thread_id].data[idx]) << " thread_id=" << thread_id << " idx=" << idx;
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
