#include <gtest/gtest.h>

#include <future>
#include <array>

#include <neutrino_transport_shared_mem_endpoint_proxy_st.hpp>
#include <neutrino_transport_shared_mem_endpoint_proxy_mt.hpp>
#include <neutrino_transport_shared_mem_win.hpp>

const std::size_t num_buffers = 5;
const DWORD buf_size = 1000;
              
const uint8_t generated_data[] = { 
//0  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15 
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 
//16 17 18 19 20 21 22 23 24 25 25  26  27  28  29  30
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//31 32 33 34 35 36 37 38 39 40 41  42  43  44  45  46
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//47 48 49 50 51 52 53 54 55 56 57  58  59  60  61  62
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//63 64 65 66 67 68 69 70 71 72 73  74  75  76  77  78
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//79 80 81 82 83 84 85 86 87 88 89  90  91  92  93  94
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//95 96 97 98 99 00 01 02 03 04 05  06  07  08  09  10
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//11 12 13 14 15 16 17 18 19 20 21  22  23  24  25  26
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//27 28 29 30 31 32 33 34 35 36 37  38  39  40  41  42
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
//43 44 45 46 47 48 49 50 51 52 53  54  55  56  57  58
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

struct CircLoop {
  const uint8_t* b;
  const uint8_t* e;
  CircLoop* n;
};

struct ProducerData
{
  CircLoop* data;
  std::size_t generated_data_len;
};

inline void link_cirl_loop(CircLoop* f, CircLoop* l) {
  l->n = f;

  CircLoop* b = f;
  while(b < l) {
    CircLoop* r = b + 1;
    b->n = r;
    b = r;
  }
}

template <std::size_t X>
const ProducerData get_generated_data() = delete;

template <>
const ProducerData get_generated_data<1>() {
  static auto ret = []() -> auto {
    static CircLoop ret[] = {
      {generated_data + 0, generated_data + 1, nullptr},
      {generated_data + 1, generated_data + 2, nullptr},
      {generated_data + 2, generated_data + 3, nullptr},
      {generated_data + 3, generated_data + 4, nullptr},
      {generated_data + 4, generated_data + 5, nullptr},
      {generated_data + 5, generated_data + 6, nullptr},
      {generated_data + 6, generated_data + 7, nullptr},
      {generated_data + 7, generated_data + 8, nullptr},
      {generated_data + 8, generated_data + 9, nullptr},
      {generated_data + 9, generated_data + 10, nullptr},
      {generated_data + 10, generated_data + 11, nullptr},
      {generated_data + 11, generated_data + 12, nullptr},
      {generated_data + 12, generated_data + 13, nullptr},
      {generated_data + 13, generated_data + 14, nullptr},
      {generated_data + 14, generated_data + 15, nullptr},
      {generated_data + 15, generated_data + 16, nullptr}
    };

    link_cirl_loop(ret, ret + (sizeof(ret) / sizeof(ret[0]) - 1));

    return ret;
  }();

  return { ret, 16 };
};

template <>
const ProducerData get_generated_data<2>() {
  static auto ret = []() -> auto {
    static CircLoop ret[] = {
      {generated_data + 0, generated_data + 2, nullptr},
      {generated_data + 2, generated_data + 4, nullptr},
      {generated_data + 4, generated_data + 6, nullptr},
      {generated_data + 6, generated_data + 8, nullptr},
      {generated_data + 8, generated_data + 10, nullptr},
      {generated_data + 10, generated_data + 12, nullptr},
      {generated_data + 12, generated_data + 14, nullptr},
      {generated_data + 14, generated_data + 16, nullptr}
    };

    link_cirl_loop(ret, ret + (sizeof(ret) / sizeof(ret[0]) - 1));

    return ret;
  }();

  return { ret, 16 };
};

template <>
const ProducerData get_generated_data<3>() {
  static auto ret = []() -> auto {
    static CircLoop ret[] = {
      {generated_data + 0, generated_data + 3, nullptr},
      {generated_data + 3, generated_data + 6, nullptr},
      {generated_data + 6, generated_data + 9, nullptr},
      {generated_data + 9, generated_data + 12, nullptr},
      {generated_data + 12, generated_data + 15, nullptr},
      {generated_data + 15, generated_data + 18, nullptr},
      {generated_data + 18, generated_data + 21, nullptr},
      {generated_data + 21, generated_data + 24, nullptr},
      {generated_data + 24, generated_data + 27, nullptr},
      {generated_data + 27, generated_data + 30, nullptr}
    };

    link_cirl_loop(ret, ret + (sizeof(ret) / sizeof(ret[0]) - 1));

    return ret;
  }();

  return { ret, 30 };
};

template <>
const ProducerData get_generated_data<5>() {
  static auto ret = []() -> auto {
    static CircLoop ret[] = {
      {generated_data + 0, generated_data + 5, nullptr},
      {generated_data + 5, generated_data + 10, nullptr},
      {generated_data + 10, generated_data + 15, nullptr},
      {generated_data + 15, generated_data + 20, nullptr},
      {generated_data + 20, generated_data + 25, nullptr},
      {generated_data + 25, generated_data + 30, nullptr},
      {generated_data + 30, generated_data + 35, nullptr},
      {generated_data + 35, generated_data + 40, nullptr},
      {generated_data + 40, generated_data + 45, nullptr},
      {generated_data + 45, generated_data + 50, nullptr},
      {generated_data + 50, generated_data + 55, nullptr},
      {generated_data + 55, generated_data + 60, nullptr},
      {generated_data + 60, generated_data + 65, nullptr},
      {generated_data + 65, generated_data + 70, nullptr},
      {generated_data + 70, generated_data + 75, nullptr},
      {generated_data + 75, generated_data + 80, nullptr},
      {generated_data + 80, generated_data + 85, nullptr},
      {generated_data + 85, generated_data + 90, nullptr}
    };

    link_cirl_loop(ret, ret + (sizeof(ret) / sizeof(ret[0]) - 1));

    return ret;
  }();

  return { ret, 90 };
};

template <>
const ProducerData get_generated_data<10>() {
  static auto ret = []() -> auto {
    static CircLoop ret[] = {
      {generated_data + 0, generated_data + 10, nullptr},
      {generated_data + 10, generated_data + 20, nullptr},
      {generated_data + 20, generated_data + 30, nullptr},
      {generated_data + 30, generated_data + 40, nullptr},
      {generated_data + 40, generated_data + 50, nullptr},
      {generated_data + 50, generated_data + 60, nullptr},
      {generated_data + 60, generated_data + 70, nullptr},
      {generated_data + 70, generated_data + 80, nullptr},
      {generated_data + 80, generated_data + 90, nullptr},
      {generated_data + 90, generated_data + 100, nullptr},
      {generated_data + 100, generated_data + 110, nullptr},
      {generated_data + 110, generated_data + 120, nullptr},
      {generated_data + 120, generated_data + 130, nullptr}
    };

    link_cirl_loop(ret, ret + (sizeof(ret) / sizeof(ret[0]) - 1));

    return ret;
  }();

  return { ret, 130 };
};

template <std::size_t num_bytes_per_single_consume_op>
void mark_array_thread_id(uint8_t thread_id, const CircLoop* data, uint8_t* data_to_consume) = delete;

template <>
void mark_array_thread_id<1>(uint8_t thread_id, const CircLoop* data, uint8_t* data_to_consume) {
  *data_to_consume = *(data->b) | thread_id;
}

template <>
void mark_array_thread_id<2>(uint8_t thread_id, const CircLoop* data, uint8_t* data_to_consume) {
  *data_to_consume = *(data->b) | thread_id;
  *(data_to_consume+1) = *(data->b + 1) | thread_id;
}

template <>
void mark_array_thread_id<3>(uint8_t thread_id, const CircLoop* data, uint8_t* data_to_consume) {
  *data_to_consume = *(data->b) | thread_id;
  *(data_to_consume + 1) = *(data->b + 1) | thread_id;
  *(data_to_consume + 2) = *(data->b + 2) | thread_id;
}

template <>
void mark_array_thread_id<5>(uint8_t thread_id, const CircLoop* data, uint8_t* data_to_consume) {
  *data_to_consume = *(data->b) | thread_id;
  *(data_to_consume + 1) = *(data->b + 1) | thread_id;
  *(data_to_consume + 2) = *(data->b + 2) | thread_id;
  *(data_to_consume + 3) = *(data->b + 3) | thread_id;
  *(data_to_consume + 4) = *(data->b + 4) | thread_id;
}

template <>
void mark_array_thread_id<10>(uint8_t thread_id, const CircLoop* data, uint8_t* data_to_consume) {
  *data_to_consume = *(data->b) | thread_id;
  *(data_to_consume + 1) = *(data->b + 1) | thread_id;
  *(data_to_consume + 2) = *(data->b + 2) | thread_id;
  *(data_to_consume + 3) = *(data->b + 3) | thread_id;
  *(data_to_consume + 4) = *(data->b + 4) | thread_id;
  *(data_to_consume + 5) = *(data->b + 5) | thread_id;
  *(data_to_consume + 6) = *(data->b + 6) | thread_id;
  *(data_to_consume + 7) = *(data->b + 7) | thread_id;
  *(data_to_consume + 8) = *(data->b + 8) | thread_id;
  *(data_to_consume + 9) = *(data->b + 9) | thread_id;
}

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

  void TearDown() override {
    neutrino::impl::memory::win_shared_mem::print_stats(std::cout);
  }

  template <std::size_t num_bytes_per_single_consume_op>
  void validate_data_transfer(neutrino::impl::transport::endpoint_proxy_t& x, const size_t num_threads) {

    const std::size_t bytes_to_consume = num_buffers * std::size_t(buf_size) * 1000;
    const std::size_t adjusted_bytes_per_thread = (bytes_to_consume / num_threads) * num_threads;
    const std::size_t adjusted_bytes_to_consume = adjusted_bytes_per_thread * num_threads;


    // Validate neutrino transport is able to transfer data from producer's endpoint proxy to consumer's endpoint.
    // Following the neutrino's logic, producer sends data to the consumer via neutrino's endpoint proxy.
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

    std::atomic<std::size_t> produced_bytes_total = 0;
    std::atomic<bool> producer_done = false;

    std::size_t consumed_bytes_total = 0;

    std::promise<void> promise_consumer_done;
    std::shared_future<void> future_consumer_done(promise_consumer_done.get_future());

    neutrino::impl::memory::win_shared_mem::v00_async_listener_t listener(host_pool);

    {
      char buf[200];
      snprintf(buf, sizeof(buf) / sizeof(buf[0]), "expect blocks %lld bytes %lld\n", bytes_to_consume / buf_size + 1, bytes_to_consume);
      std::cerr << buf;
    }

    std::vector<std::size_t> consumed_thread_current(num_threads);
    uint64_t consumed_sequence = 0;

    std::size_t generated_data_len = get_generated_data<num_bytes_per_single_consume_op>().generated_data_len;

    ////////////////////////////////////////////////////////
    //
    // register consuer procedure
    //
    ////////////////////////////////////////////////////////
    // listener.start returns a functor which, when called, instructs listener to exit
    auto at_cancel = listener.start(
      [this, adjusted_bytes_to_consume, generated_data_len, &produced_bytes_total, &producer_done, &consumed_bytes_total, &consumed_thread_current, &promise_consumer_done, &consumed_sequence](uint64_t sequence, const uint8_t* p, const uint8_t* e)
      {
        ASSERT_TRUE(p <= e) << "sequence=" << sequence << " bad block";

        if (p == nullptr) {
          std::cerr << "timeout\n";
          //if (producer_done) {
          //  std::cerr << "promise_consumer_done consumed_bytes_total=" << consumed_bytes_total << " produced_bytes_total=" << produced_bytes_total << "\n";
          //  promise_consumer_done.set_value();
          //}
          return;
        }

        ASSERT_EQ(consumed_sequence + 1, sequence) << "sequence mismatch";
        consumed_sequence = sequence;

        consumed_bytes_total += e - p;

        // this function is called when the listener gets another portion of data
        // mutiple threads are sending data, each byte is encoded 0xAABB where 0x00AA is a thread id (0..consumed_list.size()) and BB is a some number of a range 0..0x00ff
        while(p < e) {

          const auto thread_id = (*p) >> 4;
          ASSERT_TRUE(thread_id < consumed_thread_current.size()) << "sequence=" << sequence << " wrong thread_id=" << thread_id;

          auto idx = (consumed_thread_current[thread_id]++) % generated_data_len;

          ASSERT_EQ(*p, generated_data[idx]) << "sequence=" << sequence << " idx=" << idx << "thread_id=" << thread_id;
          p++;
        }

        //if(producer_done && consumed_bytes_total >= produced_bytes_total) {
        if(consumed_bytes_total >= adjusted_bytes_to_consume) {
          std::cerr << "promise_consumer_done consumed_bytes_total=" << consumed_bytes_total << " produced_bytes_total=" << produced_bytes_total << "\n";
          promise_consumer_done.set_value();
        }
      }
    );

    // all threads are blocked at the beginning until promise_start_all is fired,
    // this guarantees all threads are sending data simultaneously (depends on how OS schedules those threads)
    std::promise<void> promise_start_all;
    std::shared_future<void> shared_future_start_all(promise_start_all.get_future());
    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    ////////////////////////////////////////////////////////
    //
    // generate num_threads producers, each sending adjusted_bytes_per_thread bytes async
    // all starts simultaneously
    //
    ////////////////////////////////////////////////////////
    for (std::size_t t = 0; t < num_threads; t++)
    {
      futures.emplace_back(std::async(
        std::launch::async,
        [this, &x, &shared_future_start_all, t, adjusted_bytes_per_thread, &produced_bytes_total]()
        {
          char buf[200];

          // barrier-like code to sync multiple producers
          shared_future_start_all.wait();
          shared_future_start_all.get();

          snprintf(buf, sizeof(buf) / sizeof(buf[0]), "producer t %lld\n", t);
          std::cerr << buf;

          std::size_t consumed = 0;

          auto data = get_generated_data<num_bytes_per_single_consume_op>().data;
          uint8_t data_to_consume[num_bytes_per_single_consume_op];

          do
          {
            mark_array_thread_id<num_bytes_per_single_consume_op>(t << 4, data, data_to_consume);

            if (!x.consume(data_to_consume, data_to_consume + num_bytes_per_single_consume_op)) {
              char buf[200];
              snprintf(buf, sizeof(buf) / sizeof(buf[0]), "consume failure, thread_id=%llu, at=%lld", t, consumed);
              throw std::runtime_error(buf);
            }

            consumed += num_bytes_per_single_consume_op;
            data = data->n;
          } while (consumed < adjusted_bytes_per_thread);

          produced_bytes_total += consumed;

          snprintf(buf, sizeof(buf) / sizeof(buf[0]), "producer flush t %lld\n", t);
          std::cerr << buf;

          uint8_t flushp = 0;
          if (!x.consume(&flushp, &flushp)) {
            char buf[200];
            snprintf(buf, sizeof(buf) / sizeof(buf[0]), "flush failure, thread_id=%llu", t);
            throw std::runtime_error(buf);
          }

          snprintf(buf, sizeof(buf) / sizeof(buf[0]), "producer done t %lld\n", t);
          std::cerr << buf;
        }
      ));
    }

    ////////////////////////////////////////////////////////
    //
    // waiting for producer to complete sending data
    //
    ////////////////////////////////////////////////////////
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
      producer_done = true;
    }
    const auto producer_duration = (std::chrono::steady_clock::now() - c1) + std::chrono::seconds(2);
    
    ////////////////////////////////////////////////////////
    //
    // waiting for consumer to report expected bytes have been consumed
    // assume upper limit of consumer's duration as producer's duration doubled
    //
    ////////////////////////////////////////////////////////
    const auto consumer_status = future_consumer_done.wait_for(producer_duration + producer_duration);
    EXPECT_TRUE(consumer_status == std::future_status::ready) << "timeout consumer";

    // cancel consumer's operation
    // the consumer runs its operation in a separate thread which may hang, so we need to control its timeout
    // do canclel as async and wait on its future with a timeout
    auto cancelled = std::async(std::launch::async, [&at_cancel]() { at_cancel(); });
    const auto consumer_cancelled_status = cancelled.wait_until(std::chrono::steady_clock::now() + std::chrono::milliseconds{ 30 });
    EXPECT_TRUE(consumer_cancelled_status == std::future_status::ready) << "timeout cancelled ";

    ////////////////////////////////////////////////////////
    //
    // validate total consumed bytes match to produced bytes
    //
    ////////////////////////////////////////////////////////
    ASSERT_TRUE(consumed_bytes_total >= produced_bytes_total) << "consumed_bytes_total=" << consumed_bytes_total << " expect greater than produced_bytes_total=" << produced_bytes_total;

    ADD_FAILURE();
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
