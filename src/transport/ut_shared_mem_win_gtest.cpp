#include <gtest/gtest.h>

#include <neutrino_transport_shared_mem_win.hpp>

class neutrino_transport_shared_mem_win_Fixture : public ::testing::Test {
  public:
  const std::size_t num_buffers = 5;
  const DWORD buf_size = 1000;

  std::vector<uint8_t> expected_data;

  std::unique_ptr<neutrino::impl::memory::win_shared_mem::v00_names_t> nm;

  std::unique_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > host_pool;

  std::unique_ptr < neutrino::impl::memory::win_shared_mem::v00_pool_t > guest_pool;


  void SetUp() override {

    expected_data.reserve(buf_size);
    for (std::size_t i = 0; i < buf_size; i++)
      expected_data.emplace_back((uint8_t)(i % 0xff));

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

TEST_F(neutrino_transport_shared_mem_win_Fixture, v00_buffer_t)
{
    neutrino::impl::shared_memory::buffer_t::span_t guest_span = guest_pool->m_buffer.load()->get_span(buf_size);
    ASSERT_EQ(0, guest_span.free_bytes);
    memmove(guest_span.m_span, &(expected_data[0]), buf_size);

    ASSERT_TRUE(guest_pool->m_buffer.load()->is_clean());
    ASSERT_TRUE(host_pool->m_buffer.load()->is_clean());

    guest_pool->m_buffer.load()->dirty();
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

TEST_F(neutrino_transport_shared_mem_win_Fixture, v00_pool_t)
{
  std::vector<neutrino::impl::shared_memory::buffer_t*> bp;

  neutrino::impl::shared_memory::buffer_t* pB = guest_pool->m_buffer.load();
  for(std::size_t idx = 0; idx < num_buffers; idx++)
  {
    ASSERT_TRUE(pB->is_clean());
    neutrino::impl::shared_memory::buffer_t::span_t guest_span = pB->get_span(buf_size);
    pB->dirty();
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