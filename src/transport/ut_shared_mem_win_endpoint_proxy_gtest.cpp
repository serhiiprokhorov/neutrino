#include <gtest/gtest.h>

#include <future>

#include <neutrino_transport_shared_mem_endpoint_proxy_st.hpp>
#include <neutrino_transport_shared_mem_win.hpp>

class neutrino_transport_shared_mem_endpoint_proxy_Fixture : public ::testing::Test {
  public:
  const std::size_t num_buffers = 5;
  const DWORD buf_size = 1000;

  std::vector<uint8_t> expected_data;

  std::unique_ptr<neutrino::impl::memory::win_shared_mem::v00_names_t> nm;

  std::shared_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > host_pool;

  std::shared_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > guest_pool;


  void SetUp() override {
    
    {
      const auto sz = num_buffers * buf_size * 3000 /*big value here guarantees no errors even with MT env*/;
      expected_data.reserve(sz);
      for (std::size_t i = 0; i < sz; i++)
        expected_data.emplace_back((uint8_t)(i % 0xff));
    }

    nm.reset(new neutrino::impl::memory::win_shared_mem::v00_names_t(123, "domain", "suffix"));

    host_pool.reset( new neutrino::impl::memory::win_shared_mem::v00_pool_t(
      num_buffers
      , neutrino::impl::memory::win_shared_mem::OPEN_MODE::CREATE
      , *nm
      , buf_size));

    guest_pool.reset( new neutrino::impl::memory::win_shared_mem::v00_pool_t(
      num_buffers
      , neutrino::impl::memory::win_shared_mem::OPEN_MODE::OPEN
      , *nm
      , buf_size));

    ASSERT_TRUE(guest_pool->m_buffer.load()->is_clean());
    ASSERT_TRUE(host_pool->m_buffer.load()->is_clean());
  }
};

TEST_F(neutrino_transport_shared_mem_endpoint_proxy_Fixture, singlethread_shared_memory_endpoint_proxy_t)
{
  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t::shared_memory_endpoint_proxy_params_t params;
  params.m_message_buf_watermark = 0; // TODO: not in use
  neutrino::impl::transport::singlethread_shared_memory_endpoint_proxy_t x(params, guest_pool);

  neutrino::impl::memory::win_shared_mem::v00_async_listener_t listener(host_pool);

  std::atomic<std::ptrdiff_t> actual_consumed_cc = 0;
  std::vector<uint8_t> consumed_list;
  consumed_list.resize(expected_data.size());
  uint8_t* pStart = &(consumed_list[0]);
  uint8_t* pData = pStart;
  auto at_cancel = listener.start(
    [&consumed_list, &actual_consumed_cc, &pData, &pStart](const uint8_t* p, const uint8_t* e)
    {
      while(p != e)
      {
        //consumed_list.push_back(*p);
        *(pData++) = *(p++);
      }
      actual_consumed_cc = pData - pStart;
    }
  );

  auto f = std::async(
    std::launch::async,
    [this, &x, &actual_consumed_cc]()
    {
      const std::size_t retries_max = 10;
      std::size_t retries_cc = 0;
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
    if(actual_consumed_cc.load() >= actual_transmitted_cc)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
  }

  ASSERT_EQ(actual_consumed_cc.load(), actual_transmitted_cc);

  for(std::size_t i = 0; i < consumed_list.size(); i++)
  {
    ASSERT_EQ(consumed_list[i], expected_data[i]) << i ;
  }

  at_cancel();

}
