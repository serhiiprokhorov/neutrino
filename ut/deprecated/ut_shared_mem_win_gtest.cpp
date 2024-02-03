#include <gtest/gtest.h>

#include <neutrino_transport_shared_mem_win.hpp>

class neutrino_transport_shared_mem_win_Fixture : public ::testing::Test {
  public:
  const std::size_t num_buffers = 5;
  const DWORD buf_size = 1000;

  std::vector<uint8_t> expected_data;

  std::unique_ptr<neutrino::impl::memory::win_shared_mem::v00_names_t> nm;

  std::shared_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > host_pool;

  std::shared_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > guest_pool;


  void SetUp() override {
    
    {
      const auto sz = 30 * buf_size;
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

TEST_F(neutrino_transport_shared_mem_win_Fixture, v00_buffer_t_clean_dirty)
{
    neutrino::impl::shared_memory::buffer_t::span_t guest_span = guest_pool->m_buffer.load()->get_span(buf_size);
    ASSERT_EQ(0, guest_span.free_bytes);
    memmove(guest_span.m_span, &(expected_data[0]), buf_size);

    ASSERT_TRUE(guest_pool->m_buffer.load()->is_clean());
    ASSERT_TRUE(host_pool->m_buffer.load()->is_clean());

    guest_pool->m_buffer.load()->dirty(0);
    ASSERT_FALSE(guest_pool->m_buffer.load()->is_clean());
    ASSERT_FALSE(guest_pool->m_buffer.load()->is_clean());
    ASSERT_FALSE(host_pool->m_buffer.load()->is_clean());
    ASSERT_FALSE(host_pool->m_buffer.load()->is_clean());

    neutrino::impl::shared_memory::buffer_t* pNext = guest_pool->next_available(guest_pool->m_buffer.load());
    ASSERT_FALSE(pNext == guest_pool->m_buffer.load()) << "guest_pool.m_buffer has been declared dirty, the pool is expected to pick up next";

    neutrino::impl::shared_memory::buffer_t::span_t host_span = host_pool->m_buffer.load()->get_data();

    ASSERT_EQ(buf_size, host_span.free_bytes);
    ASSERT_EQ(0, memcmp(host_span.m_span, guest_span.m_span, buf_size));

    host_pool->m_buffer.load()->clear();
    ASSERT_TRUE(guest_pool->m_buffer.load()->is_clean());
    ASSERT_TRUE(host_pool->m_buffer.load()->is_clean());
}

TEST_F(neutrino_transport_shared_mem_win_Fixture, v00_pool_t_next_available)
{
  std::vector<neutrino::impl::shared_memory::buffer_t*> bp;

  uint64_t dirty_count{0};
  neutrino::impl::shared_memory::buffer_t* pB = guest_pool->m_buffer.load();
  for(std::size_t idx = 0; idx < num_buffers; idx++)
  {
    ASSERT_TRUE(pB->is_clean());
    neutrino::impl::shared_memory::buffer_t::span_t guest_span = pB->get_span(buf_size);
    pB->dirty(dirty_count++);
    bp.push_back(pB);
    neutrino::impl::shared_memory::buffer_t* pNext = guest_pool->next_available(pB);
    if(idx == num_buffers - 1)
    {
      ASSERT_TRUE(pNext == pB) << "expect the pool to cycle back on same buffer when no more free buffers are available";
      break;
    }
    pB = pNext;
    for (std::size_t uniq_check_idx = 0; uniq_check_idx < bp.size(); uniq_check_idx++)
    {
      ASSERT_TRUE(pB != bp[uniq_check_idx]) << idx << " is duplicated at " << uniq_check_idx;
    }
  }

  for(std::size_t dirty_idx = 0; dirty_idx < bp.size(); dirty_idx++)
  {
    neutrino::impl::shared_memory::buffer_t* pNext = guest_pool->next_available(bp[dirty_idx]);
    ASSERT_TRUE(pNext == bp[dirty_idx]) << "expect buffer to check all buffers and return back to the same one when all of them are dirty";
  }
}

TEST_F(neutrino_transport_shared_mem_win_Fixture, v00_pool_t_linear_transfer)
{
  // validate neutrino::impl::memory::win_shared_mem::v00_pool_t and neutrino::impl::memory::win_shared_mem::v00_async_listener_t and neutrino::impl::shared_memory::buffer_t
  // are capable to transfer data linearly, with no missed/corrupt data


  std::list<std::vector<uint8_t>> consumed_list;

  neutrino::impl::memory::win_shared_mem::v00_async_listener_t consumer(host_pool);

  const uint32_t expected_buffers_received = expected_data.size() / buf_size;
  std::atomic_uint32_t actual_buffers_received{0};

  std::string actual_sequences;
  actual_sequences.reserve(size_t(expected_buffers_received) * 20);

  std::string expected_sequences;
  expected_sequences.reserve(actual_sequences.capacity());

  for(uint32_t x = 1; x <= expected_buffers_received; x++)
    expected_sequences.append(",").append(std::to_string(x));

  auto at_cancel = consumer.start(
    [&actual_sequences, &consumed_list, &actual_buffers_received](uint64_t sequence, const uint8_t* p, const uint8_t* e)
    {
      consumed_list.emplace_back();
      consumed_list.back().assign(p, e);
      actual_buffers_received++;
      actual_sequences.append(",").append(std::to_string(sequence));
    }
  );

  uint64_t dirty_count{0};
  std::size_t transmitted = 0;
  auto timepoint_of_timeout = std::chrono::steady_clock::now() + std::chrono::seconds{ 40 };
  for(std::size_t i = 0; i < expected_buffers_received; i++)
  {
    if(guest_pool->m_buffer.load()->is_clean())
    {
      // TODO: modify expected_data to check linear transfer
      neutrino::impl::shared_memory::buffer_t::span_t guest_span = guest_pool->m_buffer.load()->get_span(buf_size);
      memmove(guest_span.m_span, &(expected_data[transmitted]), buf_size);
      guest_pool->m_buffer.load()->dirty(++dirty_count);
      transmitted += buf_size;
    }
    guest_pool->m_buffer = guest_pool->next_available(guest_pool->m_buffer.load());

    std::cout << (guest_pool->m_buffer.load()->is_clean() ? "c\n" : "d\n");
    // this sleep allows consumer thread to get some processing time
    std::this_thread::sleep_for(std::chrono::milliseconds{1});

    ASSERT_TRUE(timepoint_of_timeout > std::chrono::steady_clock::now());
  }

  while(actual_buffers_received.load() < expected_buffers_received)
  {
    // this sleep allows consumer thread to get some processing time
    ASSERT_TRUE(timepoint_of_timeout > std::chrono::steady_clock::now());
    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
  }

  at_cancel();

  ASSERT_TRUE(timepoint_of_timeout > std::chrono::steady_clock::now());

  EXPECT_EQ(actual_buffers_received.load(), expected_buffers_received);
  EXPECT_EQ(actual_sequences, expected_sequences);

  std::size_t consumed = 0;
  std::size_t block = 0;
  for(const auto& c : consumed_list)
  {
    for(std::size_t x = 0; x < c.size(); x++)
    {
      ASSERT_TRUE(expected_data[consumed++] == c[x]) << "consumed " << (consumed - 1) << ", block " << block << " x " << x;
    }
    block++;
  }
}
